/*
 * Copyright (C) 2018  Christian Berger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
#include <libyuv.h>
#include <X11/Xlib.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{1};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("cid")) ||
         (0 == commandlineArguments.count("name")) ) {
        std::cerr << argv[0] << " listens for VP8 or VP9 frames in an OD4Session to decode as ARGB image data into a shared memory area." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --cid=<OpenDaVINCI session> --name=<name of shared memory area> [--verbose]" << std::endl;
        std::cerr << "         --cid:     CID of the OD4Session to listen for VP8 or VP9 frames" << std::endl;
        std::cerr << "         --id:      when using several instances, only decode VP8 or VP9 with this senderStamp" << std::endl;
        std::cerr << "         --name:    name of the shared memory area to create" << std::endl;
        std::cerr << "         --verbose: print decoding information and display image" << std::endl;
        std::cerr << "Example: " << argv[0] << " --cid=111 --name=data --verbose" << std::endl;
    }
    else {
        const std::string NAME{commandlineArguments["name"]};
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};
        const uint32_t ID{(commandlineArguments["id"].size() != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["id"])) : 0};

        // Interface to a running OpenDaVINCI session (ignoring any incoming Envelopes).
        cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};

        std::unique_ptr<cluon::SharedMemory> sharedMemory(nullptr);

        std::atomic<bool> running{true};
        vpx_codec_ctx_t codec;
        Display *display{nullptr};
        Visual *visual{nullptr};
        Window window{0};
        XImage *ximage{nullptr};

        auto onNewImage = [&running, &codec, &sharedMemory, &display, &visual, &window, &ximage, &NAME, &VERBOSE, &ID](cluon::data::Envelope &&env){
            if (ID == env.senderStamp()) {
                opendlv::proxy::ImageReading img = cluon::extractMessage<opendlv::proxy::ImageReading>(std::move(env));

                // TODO: Check for switching format in between frames.
                if ( ("VP80" == img.format()) || ("VP90" == img.format()) ) {
                    const uint32_t WIDTH = img.width();
                    const uint32_t HEIGHT = img.height();

                    if (!sharedMemory) {
                        vpx_codec_err_t result{};
                        memset(&codec, 0, sizeof(codec));
                        if ("VP80" == img.format()) {
                            result = vpx_codec_dec_init(&codec, &vpx_codec_vp8_dx_algo, nullptr, 0);
                            if (!result) {
                                std::clog << "[opendlv-video-vpx-decoder]: Using " << vpx_codec_iface_name(&vpx_codec_vp8_dx_algo) << std::endl;
                            }
                        }
                        if ("VP90" == img.format()) {
                            result = vpx_codec_dec_init(&codec, &vpx_codec_vp9_dx_algo, nullptr, 0);
                            if (!result) {
                                std::clog << "[opendlv-video-vpx-decoder]: Using " << vpx_codec_iface_name(&vpx_codec_vp9_dx_algo) << std::endl;
                            }
                        }
                        if (result) {
                            std::cerr << "[opendlv-video-vpx-decoder]: Failed to initialize decoder: " << vpx_codec_err_to_string(result) << std::endl;
                            running.store(false);
                        }
                        else {
                            sharedMemory.reset(new cluon::SharedMemory{NAME, WIDTH * HEIGHT * 4});
                            std::clog << "[opendlv-video-vpx-decoder]: Created shared memory " << NAME << " (" << (WIDTH * HEIGHT * 4) << " bytes) for an ARGB image (width = " << WIDTH << ", height = " << HEIGHT << ")." << std::endl;

                            if (!sharedMemory && !sharedMemory->valid()) {
                                std::cerr << "[opendlv-video-vpx-decoder]: Failed to create shared memory." << std::endl;
                                running.store(false);
                            }

                            if (VERBOSE) {
                                display = XOpenDisplay(NULL);
                                visual = DefaultVisual(display, 0);
                                window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, WIDTH, HEIGHT, 1, 0, 0);
                                ximage = XCreateImage(display, visual, 24, ZPixmap, 0, reinterpret_cast<char*>(sharedMemory->data()), WIDTH, HEIGHT, 32, 0);
                                XMapWindow(display, window);
                            }
                        }
                    }
                    if (sharedMemory) {
                        vpx_codec_iter_t it{nullptr};
                        vpx_image_t *yuvFrame{nullptr};

                        std::string data{img.data()};
                        const uint32_t LEN{static_cast<uint32_t>(data.size())};

                        if (vpx_codec_decode(&codec, reinterpret_cast<const unsigned char*>(data.c_str()), LEN, nullptr, 0)) {
                            std::cerr << "[opendlv-video-vpx-decoder]: Decoding for current frame failed." << std::endl;
                        }
                        else {
                            while (nullptr != (yuvFrame = vpx_codec_get_frame(&codec, &it))) {
                                sharedMemory->lock();
                                {
                                    libyuv::I420ToARGB(yuvFrame->planes[VPX_PLANE_Y], yuvFrame->stride[VPX_PLANE_Y], yuvFrame->planes[VPX_PLANE_U], yuvFrame->stride[VPX_PLANE_U], yuvFrame->planes[VPX_PLANE_V], yuvFrame->stride[VPX_PLANE_V], reinterpret_cast<uint8_t*>(sharedMemory->data()), WIDTH * 4, WIDTH, HEIGHT);
                                    if (VERBOSE) {
                                        XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
                                    }
                                }
                                sharedMemory->unlock();
                                sharedMemory->notifyAll();
                            }
                        }
                    }
                }
            }
        };

        // Register lambda to handle incoming frames.
        od4.dataTrigger(opendlv::proxy::ImageReading::ID(), onNewImage);

        while (od4.isRunning() && running.load()) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1s);
        }

        vpx_codec_destroy(&codec);

        if (VERBOSE) {
            XCloseDisplay(display);
        }
        retCode = 0;
    }
    return retCode;
}

