#!/bin/bash

echo "观战模式启动中"
echo "模式: Python AI（红色）vs 规则AI（蓝色）"
echo "速度: 正常（有渲染）"
echo "窗口: SFML图形界面"
echo ""

cd "$(dirname "$0")"

# 检查模型
if [ ! -f "python/policy_model.pth" ]; then
    echo " 生成初始AI模型"
    cd python && python3 train.py && cd ..
fi

# Team 0 = 蓝色（AI规则），Team 1 = 红色（Python AI）
./cmake-build-release/DS_PJ --mode ai_vs_ai --team0 ai_rule --team1 ai_python
