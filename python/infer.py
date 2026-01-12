"""
推理脚本 - 用于游戏运行时的AI决策
读取游戏状态JSON，使用训练好的模型进行推理，返回动作JSON
"""

import sys
import json
import numpy as np
import os

# 尝试导入PyTorch，如果没有安装则使用随机策略
try:
    import torch
    import torch.nn as nn
    TORCH_AVAILABLE = True
except ImportError:
    TORCH_AVAILABLE = False
    print("Warning: PyTorch not installed, using random policy", file=sys.stderr)


# 网络超参数（与train.py保持一致）
HIDDEN_SIZE_1 = 768
HIDDEN_SIZE_2 = 512
HIDDEN_SIZE_3 = 256
DROPOUT_RATE_1 = 0.2
DROPOUT_RATE_2 = 0.1


class PolicyNetwork(nn.Module):
    """策略网络 - 多头输出（与train.py完全一致）"""
    def __init__(self):
        super().__init__()
        self.input_norm = nn.BatchNorm1d(79)

        self.shared = nn.Sequential(
            nn.Linear(79, HIDDEN_SIZE_1),
            nn.Mish(),
            nn.BatchNorm1d(HIDDEN_SIZE_1),
            nn.Dropout(DROPOUT_RATE_1),

            nn.Linear(HIDDEN_SIZE_1, HIDDEN_SIZE_2),
            nn.Mish(),
            nn.BatchNorm1d(HIDDEN_SIZE_2),
            nn.Dropout(DROPOUT_RATE_1),

            nn.Linear(HIDDEN_SIZE_2, HIDDEN_SIZE_3),
            nn.Mish(),
            nn.Dropout(DROPOUT_RATE_2)
        )

        self.action_type_head = nn.Linear(HIDDEN_SIZE_3, 2)
        self.base_id_head = nn.Linear(HIDDEN_SIZE_3, 3)
        self.unit_type_head = nn.Linear(HIDDEN_SIZE_3, 5)
    
    def forward(self, state):
        x = self.input_norm(state)
        shared_features = self.shared(x)

        action_type_logits = self.action_type_head(shared_features)
        base_id_logits = self.base_id_head(shared_features)
        unit_type_logits = self.unit_type_head(shared_features)
        
        return action_type_logits, base_id_logits, unit_type_logits


def parse_state_to_features(state_json):
    """从 state 字典提取 79 维特征向量（与 train.py 完全一致）"""
    features = []

    # 1. 能量 (2)
    features.append(state_json.get("my_energy", 0) / 1000.0)
    features.append(state_json.get("enemy_energy", 0) / 1000.0)

    # 2. 基地总HP (2)
    features.append(state_json.get("my_total_base_hp", 0) / 50000.0)
    features.append(state_json.get("enemy_total_base_hp", 0) / 50000.0)

    # 3. 士兵总数 (2)
    features.append(state_json.get("my_soldier_count", 0) / 100.0)
    features.append(state_json.get("enemy_soldier_count", 0) / 100.0)

    # 4. 我方各兵种数量 (5维：只有5种兵)
    my_types = state_json.get("my_soldier_types", {})
    features.append(my_types.get("archer_count", 0) / 20.0)
    features.append(my_types.get("infantry_count", 0) / 20.0)
    features.append(my_types.get("cavalry_count", 0) / 20.0)
    features.append(my_types.get("caster_count", 0) / 20.0)
    features.append(my_types.get("doctor_count", 0) / 20.0)

    # 5. 敌方各兵种数量 (5维)
    enemy_types = state_json.get("enemy_soldier_types", {})
    features.append(enemy_types.get("archer_count", 0) / 20.0)
    features.append(enemy_types.get("infantry_count", 0) / 20.0)
    features.append(enemy_types.get("cavalry_count", 0) / 20.0)
    features.append(enemy_types.get("caster_count", 0) / 20.0)
    features.append(enemy_types.get("doctor_count", 0) / 20.0)

    # 6. 我方基地信息 (3 * 7 = 21)
    my_bases = state_json.get("my_bases", [])
    for i in range(3):
        if i < len(my_bases):
            base = my_bases[i]
            features.append(base.get("hp", 0) / 15000.0)
            features.append(base.get("position_x", base.get("x", 32)) / 64.0)  # 支持两种字段名
            features.append(base.get("position_y", base.get("y", 32)) / 64.0)
            features.append(base.get("nearby_allies", 0) / 20.0)
            features.append(base.get("nearby_enemies", 0) / 20.0)
            features.append(base.get("is_under_attack", base.get("nearby_enemies", 0) > 0))
            distance = base.get("distance_to_nearest_my_base", base.get("distance_to_nearest_enemy_base", 64))
            features.append(distance / 64.0)
        else:
            features.extend([0.0] * 7)

    # 7. 敌方基地信息 (3 * 7 = 21)
    enemy_bases = state_json.get("enemy_bases", [])
    for i in range(3):
        if i < len(enemy_bases):
            base = enemy_bases[i]
            features.append(base.get("hp", 0) / 15000.0)
            features.append(base.get("position_x", base.get("x", 32)) / 64.0)
            features.append(base.get("position_y", base.get("y", 32)) / 64.0)
            features.append(base.get("nearby_allies", 0) / 20.0)
            features.append(base.get("nearby_enemies", 0) / 20.0)
            features.append(base.get("is_under_attack", 0))
            distance = base.get("distance_to_nearest_my_base", base.get("distance_to_nearest_enemy_base", 64))
            features.append(distance / 64.0)
        else:
            features.extend([0.0] * 7)

    # 8. 战场分布 (6) - 检查字段兼容性
    dist = state_json.get("soldier_distribution", {})
    if "my_front_soldier_count" in dist:
        features.append(dist.get("my_front_soldier_count", 0) / 20.0)
        features.append(dist.get("my_mid_soldier_count", 0) / 20.0)
        features.append(dist.get("my_back_soldier_count", 0) / 20.0)
        features.append(dist.get("enemy_front_soldier_count", 0) / 20.0)
        features.append(dist.get("enemy_mid_soldier_count", 0) / 20.0)
        features.append(dist.get("enemy_back_soldier_count", 0) / 20.0)
    else:
        features.append(dist.get("my_front_soldier_count", 0) / 20.0)
        features.append(dist.get("my_avg_x", 0) / 20.0)
        features.append(dist.get("my_avg_y", 0) / 20.0)
        features.append(dist.get("enemy_front_soldier_count", 0) / 20.0)
        features.append(dist.get("enemy_avg_x", 0) / 20.0)
        features.append(dist.get("enemy_avg_y", 0) / 20.0)

    # 9. 治疗量 (2)
    features.append(state_json.get("my_heal_done", 0) / 100.0)
    features.append(state_json.get("enemy_heal_done", 0) / 100.0)

    # 10. 游戏状态 (1)
    features.append(1.0 if state_json.get("game_over", False) else 0.0)

    # 补充到 79 维
    while len(features) < 79:
        features.append(0.0)

    return np.array(features[:79], dtype=np.float32)  # 确保正好 79 维


def random_policy(state_json):
    """随机策略（PyTorch未安装或模型未训练时使用）"""
    my_base_count = state_json.get("my_base_count", 1)
    my_energy = state_json.get("my_energy", 0)
    
    # 30%概率生产士兵
    if np.random.random() < 0.3 and my_energy > 50:
        return {
            "action_type": 1,
            "base_id": np.random.randint(0, max(1, my_base_count)),
            "unit_type": np.random.randint(0, 5)  # 修正：5种兵，range(0,5) = 0,1,2,3,4 (Archer/Infantry/Cavalry/Caster/Doctor)
        }
    else:
        return {
            "action_type": 0,
            "base_id": -1,
            "unit_type": -1
        }


def model_policy(state_json, model, device):
    """使用训练好的模型进行推理"""
    # 解析状态
    state_features = parse_state_to_features(state_json)
    state_tensor = torch.FloatTensor(state_features).unsqueeze(0).to(device)

    # 推理 - 设置模型为eval模式，避免BatchNorm训练模式错误
    model.eval()
    with torch.no_grad():
        action_type_logits, base_id_logits, unit_type_logits = model(state_tensor)

        # 动态调整logits：能量越多越倾向出兵
        my_energy = state_json.get("my_energy", 0)

        if my_energy > 100:
            # 能量超过100时，开始降低wait的倾向
            # action_type_logits 的shape是 [1, 2]，索引 [0, 0] 是wait，[0, 1] 是spawn
            # 原始模型倾向于强烈等待（wait logit大约1054 vs spawn logit -1053）  # 调整策略：更激进地降低wait logits
            bias = min((my_energy - 100) * 0.5, 50.0)  # 增大调整幅度：每100能量降低50分，最多降低50分
            # 直接在原始tensor上修改
            action_type_logits = action_type_logits.clone()  # 创建副本避免修改原始输出
            action_type_logits[0, 0] -= bias  # 降低"等待"的logits
            print(f"[DEBUG] Energy={my_energy}, adjusted wait logit by {-bias:.2f}", file=sys.stderr)

        # 如果能量超过300，大幅降低wait倾向（强制出兵）
        if my_energy > 300:
            action_type_logits[0, 0] -= 100.0  # 大幅降低wait分数（总共最多-150）
            print(f"[DEBUG] High energy = {my_energy}, strongly encouraging spawn", file=sys.stderr)

        # 调试输出 调整后 的logits 和 softmax 概率
        print(f"[DEBUG] adjusted action_logits: {action_type_logits[0].tolist()}", file=sys.stderr)

        action_type_probs = torch.softmax(action_type_logits, dim=-1)
        print(f"[DEBUG] action_probs: {action_type_probs[0].tolist()}", file=sys.stderr)
        print(f"[DEBUG] base_logits: {base_id_logits[0].tolist()}", file=sys.stderr)
        print(f"[DEBUG] unit_logits: {unit_type_logits[0].tolist()}", file=sys.stderr)

        # 使用 argmax 获取最可能的动作
        action_type = torch.argmax(action_type_logits, dim=-1).item()
        base_id = torch.argmax(base_id_logits, dim=-1).item()
        unit_type = torch.argmax(unit_type_logits, dim=-1).item()

    # 如果选择wait，返回wait动作
    if action_type == 0:
        print(f"[DEBUG] Model chose WAIT.", file=sys.stderr)
        return {
            "action_type": 0,
            "base_id": -1,
            "unit_type": -1
        }

    # 检查base_id是否有效
    my_base_count = state_json.get("my_base_count", 0)
    if not my_base_count:
        my_base_count = len(state_json.get("my_bases", []))

    if base_id >= my_base_count:
        base_id = 0  # 降级到第一个基地
        print(f"[DEBUG] Corrected base_id to 0 (out of range).", file=sys.stderr)

    print(f"[DEBUG] Model chose SPAWN: base={base_id}, unit={unit_type}.", file=sys.stderr)
    return {
        "action_type": 1,
        "base_id": base_id,
        "unit_type": unit_type
    }


def main():
    # 加载模型（如果存在）
    model = None
    device = torch.device('cpu')  # 默认设备

    # 使用脚本所在目录的相对路径
    script_dir = os.path.dirname(os.path.abspath(__file__))
    model_path = os.path.join(script_dir, "policy_model.pth")

    if TORCH_AVAILABLE:
        try:
            # 自动检测训练设备：优先MPS（Apple Silicon GPU），其次是CUDA，最后是CPU
            if torch.backends.mps.is_available():
                device = torch.device('mps')
                print("Using MPS (Apple Silicon GPU) for inference", file=sys.stderr)
            else:
                device = torch.device('cpu')
                print("Using CPU for inference", file=sys.stderr)

            model = PolicyNetwork().to(device)
            model.load_state_dict(torch.load(model_path, map_location=device))
            model.eval()
            print(f"Model loaded successfully on {device}", file=sys.stderr)
        except FileNotFoundError:
            print(f"Model file not found: {model_path}, using random policy", file=sys.stderr)
        except Exception as e:
            print(f"Error loading model: {e}, using random policy", file=sys.stderr)
    
    # 从stdin读取状态JSON，或从命令行参数读取文件
    if len(sys.argv) > 1:
        # 从文件读取
        with open(sys.argv[1], 'r') as f:
            state_json = json.load(f)
    else:
        # 从stdin读取
        line = sys.stdin.readline()
        if not line:
            sys.exit(0)
        state_json = json.loads(line)
    
    # 推理
    if model is not None:
        action = model_policy(state_json, model, device)
    else:
        action = random_policy(state_json)
    
    # 输出动作JSON
    print(json.dumps(action))
    sys.stdout.flush()

if __name__ == "__main__":
    main()
