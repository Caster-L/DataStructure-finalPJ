# 基于 C++ 和 AI 决策的策略游戏

基于 MacBook M2 运行

## 运行

使用 CMakeList.txt 通过 release 模式编译得到可执行文件DS_PJ

输入下面指令运行对应模式：
```bash
sh run_play.sh # 人机对战
sh run_training.sh # 积累训练数据
sh run_watch.sh # 观战模式，AI对战随机解
```

如果直接运行，则是人类对战随机解

## 训练

训练的文件在 python 文件夹内，运行 train.py 运行训练