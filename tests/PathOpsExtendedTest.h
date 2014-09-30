/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PathOpsExtendedTest_DEFINED
#define PathOpsExtendedTest_DEFINED

#include "SkBitmap.h"
#include "SkCommandLineFlags.h"
#include "SkPath.h"
#include "SkPathOpsTypes.h"
#include "SkStream.h"
#include "SkThread.h"
#include "SkThreadUtils.h"
#include "Test.h"

DECLARE_bool(runFail);

struct PathOpsThreadState;

struct TestDesc {
    void (*fun)(skiatest::Reporter*, const char* filename);
    const char* str;
};

//extern int comparePaths(const SkPath& one, const SkPath& two);
extern int comparePaths(const SkPath& one, const SkPath& two, SkBitmap& bitmap);
extern bool drawAsciiPaths(const SkPath& one, const SkPath& two, bool drawPaths);
extern void showOp(const SkPathOp op);
extern bool testPathOp(skiatest::Reporter* reporter, const SkPath& a, const SkPath& b,
                        const SkPathOp , const char* testName);
extern bool testPathFailOp(skiatest::Reporter* reporter, const SkPath& a, const SkPath& b,
                        const SkPathOp , const char* testName);
extern bool testThreadedPathOp(skiatest::Reporter* reporter, const SkPath& a, const SkPath& b,
                        const SkPathOp , const char* testName);
extern bool testSimplify(SkPath& path, bool useXor, SkPath& out, PathOpsThreadState& state,
                         const char* pathStr);
extern bool testSimplify(skiatest::Reporter* reporter, const SkPath& path, const char* filename);

void initializeTests(skiatest::Reporter* reporter, const char* testName);
void outputProgress(char* ramStr, const char* pathStr, SkPath::FillType );
void outputProgress(char* ramStr, const char* pathStr, SkPathOp op);

void RunTestSet(skiatest::Reporter* reporter, TestDesc tests[], size_t count,
                void (*firstTest)(skiatest::Reporter* , const char* filename),
                void (*stopTest)(skiatest::Reporter* , const char* filename), bool reverse);
void ShowTestArray();
void ShowTestName(PathOpsThreadState* data, int a, int b, int c, int d);
void ShowFunctionHeader(const char* name);
void ShowPath(const SkPath& path, const char* pathName);
void ShowOp(SkPathOp op, const char* pathOne, const char* pathTwo);

#endif
