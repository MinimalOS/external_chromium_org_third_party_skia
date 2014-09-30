/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmap.h"
#include "SkData.h"
#include "SkEndian.h"
#include "SkImageInfo.h"
#include "SkTextureCompressor.h"
#include "Test.h"

// TODO: Create separate tests for RGB and RGBA data once
// ASTC and ETC1 decompression is implemented.

static bool decompresses_a8(SkTextureCompressor::Format fmt) {
    switch (fmt) {
        case SkTextureCompressor::kLATC_Format:
        case SkTextureCompressor::kR11_EAC_Format:
            return true;

        default:
            return false;
    }
}

static bool compresses_a8(SkTextureCompressor::Format fmt) {
    switch (fmt) {
        case SkTextureCompressor::kLATC_Format:
        case SkTextureCompressor::kR11_EAC_Format:
        case SkTextureCompressor::kASTC_12x12_Format:
            return true;

        default:
            return false;
    }
}

/**
 * Make sure that we properly fail when we don't have multiple of four image dimensions.
 */
DEF_TEST(CompressAlphaFailDimensions, reporter) {
    SkBitmap bitmap;
    static const int kWidth = 17;
    static const int kHeight = 17;
    SkImageInfo info = SkImageInfo::MakeA8(kWidth, kHeight);

    // R11_EAC and LATC are both dimensions of 4, so we need to make sure that we
    // are violating those assumptions. And if we are, then we're also violating the
    // assumptions of ASTC, which is 12x12 since any number not divisible by 4 is
    // also not divisible by 12. Our dimensions are prime, so any block dimension
    // larger than 1 should fail.
    REPORTER_ASSERT(reporter, kWidth % 4 != 0);
    REPORTER_ASSERT(reporter, kHeight % 4 != 0);

    bool setInfoSuccess = bitmap.setInfo(info);
    REPORTER_ASSERT(reporter, setInfoSuccess);

    bitmap.allocPixels(info);
    bitmap.unlockPixels();
    
    for (int i = 0; i < SkTextureCompressor::kFormatCnt; ++i) {
        const SkTextureCompressor::Format fmt = static_cast<SkTextureCompressor::Format>(i);
        if (!compresses_a8(fmt)) {
            continue;
        }
        SkAutoDataUnref data(SkTextureCompressor::CompressBitmapToFormat(bitmap, fmt));
        REPORTER_ASSERT(reporter, NULL == data);
    }
}

/**
 * Make sure that we properly fail when we don't have the correct bitmap type.
 * compressed textures can (currently) only be created from A8 bitmaps.
 */
DEF_TEST(CompressAlphaFailColorType, reporter) {
    SkBitmap bitmap;
    static const int kWidth = 12;
    static const int kHeight = 12;
    SkImageInfo info = SkImageInfo::MakeN32Premul(kWidth, kHeight);

    // ASTC is at most 12x12, and any dimension divisible by 12 is also divisible
    // by 4, which is the dimensions of R11_EAC and LATC. In the future, we might
    // support additional variants of ASTC, such as 5x6 and 8x8, in which case this would
    // need to be updated.
    REPORTER_ASSERT(reporter, kWidth % 12 == 0);
    REPORTER_ASSERT(reporter, kHeight % 12 == 0);

    bool setInfoSuccess = bitmap.setInfo(info);
    REPORTER_ASSERT(reporter, setInfoSuccess);

    bitmap.allocPixels(info);
    bitmap.unlockPixels();

    for (int i = 0; i < SkTextureCompressor::kFormatCnt; ++i) {
        const SkTextureCompressor::Format fmt = static_cast<SkTextureCompressor::Format>(i);
        if (!compresses_a8(fmt)) {
            continue;
        }
        SkAutoDataUnref data(SkTextureCompressor::CompressBitmapToFormat(bitmap, fmt));
        REPORTER_ASSERT(reporter, NULL == data);
    }
}

/**
 * Make sure that if you compress a texture with alternating black/white pixels, and
 * then decompress it, you get what you started with.
 */
DEF_TEST(CompressCheckerboard, reporter) {
    SkBitmap bitmap;
    static const int kWidth = 48;  // We need the number to be divisible by both
    static const int kHeight = 48; // 12 (ASTC) and 16 (ARM NEON R11 EAC).
    SkImageInfo info = SkImageInfo::MakeA8(kWidth, kHeight);

    // ASTC is at most 12x12, and any dimension divisible by 12 is also divisible
    // by 4, which is the dimensions of R11_EAC and LATC. In the future, we might
    // support additional variants of ASTC, such as 5x6 and 8x8, in which case this would
    // need to be updated. Additionally, ARM NEON and SSE code paths support up to
    // four blocks of R11 EAC at once, so they operate on 16-wide blocks. Hence, the
    // valid width and height is going to be the LCM of 12 and 16 which is 4*4*3 = 48
    REPORTER_ASSERT(reporter, kWidth % 48 == 0);
    REPORTER_ASSERT(reporter, kHeight % 48 == 0);

    bool setInfoSuccess = bitmap.setInfo(info);
    REPORTER_ASSERT(reporter, setInfoSuccess);

    bitmap.allocPixels(info);
    bitmap.unlockPixels();

    // Populate bitmap
    {
        SkAutoLockPixels alp(bitmap);

        uint8_t* pixels = reinterpret_cast<uint8_t*>(bitmap.getPixels());
        REPORTER_ASSERT(reporter, pixels);
        if (NULL == pixels) {
            return;
        }

        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                if ((x ^ y) & 1) {
                    pixels[x] = 0xFF;
                } else {
                    pixels[x] = 0;
                }
            }
            pixels += bitmap.rowBytes();
        }
    }

    SkAutoMalloc decompMemory(kWidth*kHeight);
    uint8_t* decompBuffer = reinterpret_cast<uint8_t*>(decompMemory.get());
    REPORTER_ASSERT(reporter, decompBuffer);
    if (NULL == decompBuffer) {
        return;
    }

    for (int i = 0; i < SkTextureCompressor::kFormatCnt; ++i) {
        const SkTextureCompressor::Format fmt = static_cast<SkTextureCompressor::Format>(i);

        // Ignore formats for RGBA data, since the decompressed buffer
        // won't match the size and contents of the original.
        if (!decompresses_a8(fmt) || !compresses_a8(fmt)) {
            continue;
        }

        SkAutoDataUnref data(SkTextureCompressor::CompressBitmapToFormat(bitmap, fmt));
        REPORTER_ASSERT(reporter, data);
        if (NULL == data) {
            continue;
        }

        bool decompResult =
            SkTextureCompressor::DecompressBufferFromFormat(
                decompBuffer, kWidth,
                data->bytes(),
                kWidth, kHeight, fmt);
        REPORTER_ASSERT(reporter, decompResult);

        SkAutoLockPixels alp(bitmap);
        uint8_t* pixels = reinterpret_cast<uint8_t*>(bitmap.getPixels());
        REPORTER_ASSERT(reporter, pixels);
        if (NULL == pixels) {
            continue;
        }

        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                bool ok = pixels[y*bitmap.rowBytes() + x] == decompBuffer[y*kWidth + x];
                REPORTER_ASSERT(reporter, ok);
            }
        }
    }
}

/**
 * Make sure that if we pass in a solid color bitmap that we get the appropriate results
 */
DEF_TEST(CompressLATC, reporter) {

    const SkTextureCompressor::Format kLATCFormat = SkTextureCompressor::kLATC_Format;
    static const int kLATCEncodedBlockSize = 8;

    SkBitmap bitmap;
    static const int kWidth = 8;
    static const int kHeight = 8;
    SkImageInfo info = SkImageInfo::MakeA8(kWidth, kHeight);

    bool setInfoSuccess = bitmap.setInfo(info);
    REPORTER_ASSERT(reporter, setInfoSuccess);

    bitmap.allocPixels(info);
    bitmap.unlockPixels();

    int latcDimX, latcDimY;
    SkTextureCompressor::GetBlockDimensions(kLATCFormat, &latcDimX, &latcDimY);

    REPORTER_ASSERT(reporter, kWidth % latcDimX == 0);
    REPORTER_ASSERT(reporter, kHeight % latcDimY == 0);
    const size_t kSizeToBe =
        SkTextureCompressor::GetCompressedDataSize(kLATCFormat, kWidth, kHeight);
    REPORTER_ASSERT(reporter, kSizeToBe == ((kWidth*kHeight*kLATCEncodedBlockSize)/16));
    REPORTER_ASSERT(reporter, (kSizeToBe % kLATCEncodedBlockSize) == 0);

    for (int lum = 0; lum < 256; ++lum) {
        bitmap.lockPixels();
        uint8_t* pixels = reinterpret_cast<uint8_t*>(bitmap.getPixels());
        REPORTER_ASSERT(reporter, pixels);
        if (NULL == pixels) {
            bitmap.unlockPixels();
            continue;
        }

        for (int i = 0; i < kWidth*kHeight; ++i) {
            pixels[i] = lum;
        }
        bitmap.unlockPixels();

        SkAutoDataUnref latcData(
            SkTextureCompressor::CompressBitmapToFormat(bitmap, kLATCFormat));
        REPORTER_ASSERT(reporter, latcData);
        if (NULL == latcData) {
            continue;
        }

        REPORTER_ASSERT(reporter, kSizeToBe == latcData->size());

        // Make sure that it all matches a given block encoding. Since we have
        // COMPRESS_LATC_FAST defined in SkTextureCompressor_LATC.cpp, we are using
        // an approximation scheme that optimizes for speed against coverage maps.
        // That means that each palette in the encoded block is exactly the same,
        // and that the three bits saved per pixel are computed from the top three
        // bits of the luminance value.
        const uint64_t kIndexEncodingMap[8] = { 1, 7, 6, 5, 4, 3, 2, 0 };
        const uint64_t kIndex = kIndexEncodingMap[lum >> 5];
        const uint64_t kConstColorEncoding =
            SkEndian_SwapLE64(
                255 |
                (kIndex << 16) | (kIndex << 19) | (kIndex << 22) | (kIndex << 25) |
                (kIndex << 28) | (kIndex << 31) | (kIndex << 34) | (kIndex << 37) |
                (kIndex << 40) | (kIndex << 43) | (kIndex << 46) | (kIndex << 49) |
                (kIndex << 52) | (kIndex << 55) | (kIndex << 58) | (kIndex << 61));

        const uint64_t* blockPtr = reinterpret_cast<const uint64_t*>(latcData->data());
        for (size_t i = 0; i < (kSizeToBe/8); ++i) {
            REPORTER_ASSERT(reporter, blockPtr[i] == kConstColorEncoding);
        }
    }
}
