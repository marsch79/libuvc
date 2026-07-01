/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (C) 2010-2012 Ken Tossell
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the author nor other contributors may be
*     used to endorse or promote products derived from this software
*     without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <sys/select.h>
#include <opencv2/highgui/highgui_c.h>

#include "libuvc/libuvc.h"

#define UVCTEST_SAVE_RAW15

#ifdef UVCTEST_SAVE_RAW15
  FILE *fp = NULL;
#endif

void cb(uvc_frame_t *frame, void *ptr) {
  uvc_frame_t *bgr;
  uvc_error_t ret;
  IplImage* cvImg;
  printf("callback! length = %zu, ptr = %p\n", frame->data_bytes, ptr);

  /* Optional: save raw 15-bit grayscale values to gray145raw file.
   * Enable by defining UVCTEST_SAVE_RAW14 before compilation.
   * Each pixel is written as a 16-bit little-endian value with upper 2 bits masked off.
   */

#ifdef UVCTEST_SAVE_RAW15
  if (frame->frame_format == UVC_FRAME_FORMAT_GRAY16) {
    uint8_t *p = frame->data;
    size_t pixels = (size_t)frame->width * (size_t)frame->height;
    for (size_t i = 0; i < pixels; ++i) {
      uint16_t v = (uint16_t)(p[0] | (p[1] << 8));
      v &= 0x7FFF; /* keep only lower 15 bits */
      uint8_t out[2] = { (uint8_t)(v & 0xFF), (uint8_t)(v >> 8) };
      if (fp) fwrite(out, 1, 2, fp);
      p += 2;
    }
  }
#endif

  bgr = uvc_allocate_frame(frame->width * frame->height * 3);
  if (!bgr) {
    printf("unable to allocate bgr frame!");
    return;
  }

  ret = uvc_any2bgr(frame, bgr);
  if (ret) {
    uvc_perror(ret, "uvc_any2bgr");
    uvc_free_frame(bgr);
    return;
  }

  cvImg = cvCreateImageHeader(
      cvSize(bgr->width, bgr->height),
      IPL_DEPTH_8U,
      3);

  cvSetData(cvImg, bgr->data, bgr->step); 

  cvNamedWindow("Test", CV_WINDOW_AUTOSIZE);
  cvShowImage("Test", cvImg);
  cvWaitKey(10);

  cvReleaseImageHeader(&cvImg);

  uvc_free_frame(bgr);
}

int main(int argc, char **argv) {
  uvc_context_t *ctx;
  uvc_error_t res;
  uvc_device_t *dev;
  uvc_device_handle_t *devh;
  uvc_stream_ctrl_t ctrl;

#ifdef UVCTEST_SAVE_RAW15
  char name[] = "gray15.raw";
  fp = fopen(name, "wb");
  if (!fp) {
    fprintf(stderr, "warning: failed to open %s for writing\n", name);
  } else {
    printf("store raw file %s\n", name);
  }
#endif

  res = uvc_init(&ctx, NULL);

  if (res < 0) {
    uvc_perror(res, "uvc_init");
    return res;
  }

  puts("UVC initialized");

  res = uvc_find_device(
      ctx, &dev,
      0x2ac1, 0xfd00, NULL);

  if (res < 0) {
    uvc_perror(res, "uvc_find_device");
  } else {
    puts("Device found");

    res = uvc_open(dev, &devh);

    if (res < 0) {
      uvc_perror(res, "uvc_open");
    } else {
      puts("Device opened");

      uvc_print_diag(devh, stderr);

      res = uvc_get_stream_ctrl_format_size(
          devh, &ctrl, UVC_FRAME_FORMAT_GRAY16, 328, 248, 105
          //devh, &ctrl, UVC_FRAME_FORMAT_YUYV, 1920, 1080, 5          
      );

      uvc_print_stream_ctrl(&ctrl, stderr);

      if (res < 0) {
        uvc_perror(res, "get_mode");
      } else {
        res = uvc_start_streaming(devh, &ctrl, cb, (void *)12345, 0);

        if (res < 0) {
          uvc_perror(res, "start_streaming");
        } else {
          puts("Streaming started; press Enter to stop...");

          fd_set readfds;
          FD_ZERO(&readfds);
          FD_SET(STDIN_FILENO, &readfds);

          if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
          }

          uvc_stop_streaming(devh);
          puts("Done streaming.");
        }
      }

      uvc_close(devh);
      puts("Device closed");
    }

    uvc_unref_device(dev);
  }

  uvc_exit(ctx);
  puts("UVC exited");

#ifdef UVCTEST_SAVE_RAW15
  fclose(fp);
#endif

  return 0;
}

