/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SamplePipeControllers.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkGPipe.h"
#include "SkPaint.h"
#include "SkShader.h"
#include "Test.h"

// Ensures that the pipe gracefully handles drawing an invalid bitmap.
static void testDrawingBadBitmap(SkCanvas* pipeCanvas) {
    SkBitmap badBitmap;
    badBitmap.setConfig(SkImageInfo::Make(5, 5, kUnknown_SkColorType,
                                          kPremul_SkAlphaType));
    pipeCanvas->drawBitmap(badBitmap, 0, 0);
}

// Ensure that pipe gracefully handles attempting to draw after endRecording is called on the
// SkGPipeWriter.
static void testDrawingAfterEndRecording(SkCanvas* canvas) {
    PipeController pc(canvas);
    SkGPipeWriter writer;
    SkCanvas* pipeCanvas = writer.startRecording(&pc, SkGPipeWriter::kCrossProcess_Flag);
    writer.endRecording();

    SkBitmap bm;
    bm.allocN32Pixels(2, 2);
    bm.eraseColor(SK_ColorTRANSPARENT);

    SkShader* shader = SkShader::CreateBitmapShader(bm, SkShader::kClamp_TileMode,
                                                    SkShader::kClamp_TileMode);
    SkPaint paint;
    paint.setShader(shader)->unref();
    pipeCanvas->drawPaint(paint);

    pipeCanvas->drawBitmap(bm, 0, 0);
}

DEF_TEST(Pipe, reporter) {
    SkBitmap bitmap;
    bitmap.setConfig(SkImageInfo::MakeN32Premul(64, 64));
    SkCanvas canvas(bitmap);

    PipeController pipeController(&canvas);
    SkGPipeWriter writer;
    SkCanvas* pipeCanvas = writer.startRecording(&pipeController);
    testDrawingBadBitmap(pipeCanvas);
    writer.endRecording();

    testDrawingAfterEndRecording(&canvas);
}
