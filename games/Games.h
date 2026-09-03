#pragma once
// 游戏清单：新游戏在这里 include 并加入 kGames 数组（唯一需要动的地方）
#include "core/Game.h"
#include "games/Example.hpp"
#include "games/texun.hpp"

// 所有已接入游戏（UI 下拉框按此顺序展示）
inline Game *const kGames[] = {
    &example::game, // 教学接入模板（占位偏移，按具体游戏修改）
    &texun::game,   // 枪战特训完整接入实例
};
inline const int kGameCount = sizeof(kGames) / sizeof(kGames[0]);
