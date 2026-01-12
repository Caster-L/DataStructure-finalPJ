#!/bin/bash

# 默认运行8000局，可通过参数调整
NUM_GAMES=${1:8000}

echo "模式: 规则AI（蓝色）vs 规则AI（红色）"
echo "游戏局数: $NUM_GAMES"

cd "$(dirname "$0")"

# 检查模型
if [ ! -f "python/policy_model.pth" ]; then
    echo " 生成初始AI模型..."
    cd python && python3 train.py && cd ..
fi

# 记录开始时间
START_TIME=$(date +%s)

# 循环运行多局游戏
for i in $(seq 1 $NUM_GAMES); do
    echo ""
    echo "========================================="
    echo "  第 $i/$NUM_GAMES 局游戏开始"
    echo "========================================="
    
    # 使用双规则AI进行极速训练
    # 设置环境变量以禁用详细输出
    TRAINING_MODE=1 ./cmake-build-release/DS_PJ --mode training --team0 ai_rule --team1 ai_rule
    
    # 检查是否被中断
    if [ $? -ne 0 ]; then
        echo ""
        echo "游戏异常退出"
        break
    fi
    
    echo "第 $i 局完成"
done

# 计算总用时
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
MINUTES=$((ELAPSED / 60))
SECONDS=$((ELAPSED % 60))

echo ""
echo "========================================="
echo "  训练完成！"
echo "========================================="
echo "总局数: $i 局"
echo "总用时: ${MINUTES}分${SECONDS}秒"
echo "日志文件: game_log.json"
echo ""
