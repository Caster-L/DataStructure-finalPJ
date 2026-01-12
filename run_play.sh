#!/bin/bash

echo "人机对战模式启动中"
echo "======================"
echo "模式: 你（蓝色）vs AI模型（红色）"
echo ""
echo "操作说明:"
echo "  1. 使用左下角面板选择基地 (按1-3)"
echo "  2. 点击兵种按钮购买士兵 (A/S/D/F/G)"
echo "  3. 士兵自动移动和战斗"
echo "  4. 摧毁所有敌方基地获胜！"
echo ""
echo "按任意键开始..."
read -n 1

cd "$(dirname "$0")"
# Team 0 = 蓝色（人类控制），Team 1 = 红色（AI模型）
./cmake-build-release/DS_PJ --mode human_vs_ai --team0 human --team1 ai_python
