# MyEngine_v1
返答は日本語で行ってください。


## 命名規則の方針
includeの順番は
0. 自身の.h(cppファイルのみ)
1. 標準ライブラリ(.hなし)
2. 標準ライブラリ(.hあり)
3. 外部ライブラリ(.hなし)
4. 外部ライブラリ(.hあり)
5. 自作ライブラリ(.hなし)
6. 自作ライブラリ(.hあり)

例:
#pragma once
#include "Camera.h"(自身のペアのhファイル。cppのみ)

#include <vector>
#include <iostream>

#include <stdio.h>

#include <externals/external.h>

#include "myengine.h"


## 方針
- 複雑な実装だと判断したものは、解説を挟む
- STLで表現できるものはSTLですること。例: float kPI = 3.14159265358979323846f; ではなく float kPI = std::numbers::pi_v<float>;