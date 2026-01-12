# 训练配置
EPOCHS = 200

# 网络结构
HIDDEN_SIZE_1 = 768
HIDDEN_SIZE_2 = 512
HIDDEN_SIZE_3 = 256
DROPOUT_RATE_1 = 0.2
DROPOUT_RATE_2 = 0.1

# 优化器配置
BATCH_SIZE = 2048
LEARNING_RATE_MIN = 5e-5
LEARNING_RATE_MAX = 1e-3
WEIGHT_DECAY = 1e-5
GRADIENT_CLIP = 1.0

# 学习率调度器
LR_PCT_START = 0.05
LR_DIV_FACTOR = 10.0
LR_FINAL_DIV_FACTOR = 10000.0

# 损失权重
LOSS_WEIGHT_ACTION = 1.0
LOSS_WEIGHT_BASE = 0.5
LOSS_WEIGHT_UNIT = 0.5

EPOCHS = 400
EARLY_STOP_PATIENCE = 20
FILTER_WINNING_ONLY = True
MIN_GAME_LENGTH = 10

# 奖励折扣参数
GAMMA = 0.99  # 折扣因子
REWARD_WIN = 1.0  # 胜利奖励
REWARD_LOSS = -1.0  # 失败奖励
SAMPLING_POWER = 2.0  # 采样权重指数，越大越倾向于高奖励样本


import os
import time
import sys
from pathlib import Path
import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np

SCRIPT_DIR = Path(__file__).parent
MODEL_PATH = SCRIPT_DIR / "policy_model.pth"

class PolicyNetwork(nn.Module):
    """
    策略网络 - 包含输入归一化层 (Input Normalization)
    """
    def __init__(self):
        super().__init__()

        # 输入归一化层
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
        self.base_id_head = nn.Linear(HIDDEN_SIZE_3, 3)  # 3个基地（Constants.h中BASE_COUNT_PER_TEAM = 3）
        self.unit_type_head = nn.Linear(HIDDEN_SIZE_3, 5)  # 5种兵种(Archer/Infantry/Cavalry/Caster/Doctor)

    def forward(self, state):
        # 先进行归一化
        x = self.input_norm(state)
        # 共享层提取特征
        shared_features = self.shared(x)

        return (self.action_type_head(shared_features),
                self.base_id_head(shared_features),
                self.unit_type_head(shared_features))

def compute_discounted_rewards(episodes, winner, gamma=GAMMA):
    """
    计算每个episode的折扣累积奖励
    使用公式: R_t = r_t + gamma * r_{t+1} + gamma^2 * r_{t+2} + ... + gamma^{T-t} * r_T
    其中终局奖励为1(胜利)或-1(失败)
    """
    num_turns = len(episodes)
    if num_turns == 0:
        return []
    
    # 确定终局奖励
    final_reward = REWARD_WIN if winner == 1 else REWARD_LOSS
    
    # 从后往前计算折扣累积奖励
    discounted_rewards = []
    cumulative_reward = 0.0
    
    for i in range(num_turns - 1, -1, -1):
        if i == num_turns - 1:
            # 最后一个turn，直接使用终局奖励
            cumulative_reward = final_reward
        else:
            # 累积奖励 = gamma * 下一步的累积奖励
            cumulative_reward = gamma * cumulative_reward
        
        discounted_rewards.append(cumulative_reward)
    
    # 反转回正序
    discounted_rewards.reverse()
    
    return discounted_rewards


def sample_by_rewards(states, action_types, base_ids, unit_types, rewards, power=SAMPLING_POWER):
    """
    根据折扣累积奖励进行重采样
    奖励越高的样本被采样的次数越多
    
    Args:
        states, action_types, base_ids, unit_types: 原始数据
        rewards: 折扣累积奖励列表
        power: 采样权重指数，越大越倾向于高奖励样本
    
    Returns:
        重采样后的数据
    """
    if len(rewards) == 0:
        return states, action_types, base_ids, unit_types, rewards
    
    # 将奖励转为非负权重 (平移+缩放)
    rewards_array = np.array(rewards)
    min_reward = rewards_array.min()
    
    # 平移到非负区间
    weights = rewards_array - min_reward + 1e-8
    
    # 应用指数增强高权重样本
    weights = np.power(weights, power)
    
    # 归一化为概率分布
    weights = weights / weights.sum()
    
    # 计算采样次数（基于权重比例）
    num_samples = len(states)
    target_samples = int(num_samples * 1.5)  # 扩充50%样本
    
    # 根据权重重采样索引
    sampled_indices = np.random.choice(
        num_samples,
        size=target_samples,
        replace=True,
        p=weights
    )
    
    # 重采样数据
    sampled_states = [states[i] for i in sampled_indices]
    sampled_action_types = [action_types[i] for i in sampled_indices]
    sampled_base_ids = [base_ids[i] for i in sampled_indices]
    sampled_unit_types = [unit_types[i] for i in sampled_indices]
    sampled_rewards = [rewards[i] for i in sampled_indices]
    
    print(f"   Sampling stats:")
    print(f"   Original samples: {num_samples}")
    print(f"   Resampled: {target_samples}")
    print(f"   Reward range: [{rewards_array.min():.3f}, {rewards_array.max():.3f}]")
    print(f"   Weight range: [{weights.min():.6f}, {weights.max():.6f}]")
    
    return sampled_states, sampled_action_types, sampled_base_ids, sampled_unit_types, sampled_rewards


def load_or_process_data(log_file, device):
    # 解析JSON文件
    log_path = Path(log_file)
    if not log_path.exists():
        log_path = SCRIPT_DIR.parent / log_file

    print(f" Parsing {log_path}...")
    states = []
    action_types = []
    base_ids = []
    unit_types = []
    rewards = []

    if not log_path.exists():
        print(f" File not found: {log_path}")
        return None

    try:
        import json
        with open(log_path, 'r') as f:
            data = json.load(f)

        episodes = []
        if 'games' in data:
            skipped_games = 0
            kept_games = 0
            for game in data['games']:
                winner = game.get('summary', {}).get('winner', -1)
                
                if FILTER_WINNING_ONLY:
                    # 假设我们只学习 my_team (team1) 的胜利数据
                    if winner != 1: 
                        skipped_games += 1
                        continue
                
                kept_games += 1
                
                # 计算这局游戏中每个turn的折扣累积奖励
                game_episodes = game['episodes']
                game_rewards = compute_discounted_rewards(game_episodes, winner, GAMMA)
                
                # 将这局游戏的episodes和对应的奖励添加到总列表
                episodes.extend(game_episodes)
                rewards.extend(game_rewards)
            
            print(f" Filtered games: Kept {kept_games}, Skipped {skipped_games} (Winning only: {FILTER_WINNING_ONLY})")
        else:
            episodes = data.get('episodes', [])
            # 如果没有games结构，假设全部是胜利局（向后兼容）
            rewards = compute_discounted_rewards(episodes, winner=1, gamma=GAMMA)

        print(f"️  Found {len(episodes)} total episodes across {len(data.get('games', []))} games")

        for idx, episode in enumerate(episodes):
            # 提取我方状态（假设my_team=1）
            state_json = episode['state']

            # 提取状态特征（79维）
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
                    features.append(base.get("position_x", base.get("x", 32)) / 64.0)
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

            # 8. 战场分布 (6)
            dist = state_json.get("soldier_distribution", {})
            features.append(dist.get("my_front_soldier_count", 0) / 20.0)
            features.append(dist.get("my_avg_x", 10.0) / 20.0)
            features.append(dist.get("my_avg_y", 10.0) / 20.0)
            features.append(dist.get("enemy_front_soldier_count", 0) / 20.0)
            features.append(dist.get("enemy_avg_x", 10.0) / 20.0)
            features.append(dist.get("enemy_avg_y", 10.0) / 20.0)

            # 9. 治疗量 (2)
            features.append(state_json.get("my_heal_done", 0) / 100.0)
            features.append(state_json.get("enemy_heal_done", 0) / 100.0)

            # 10. 游戏状态 (1)
            features.append(1.0 if state_json.get("game_over", False) else 0.0)

            # 补充到 79 维
            while len(features) < 79:
                features.append(0.0)

            state_tensor = torch.FloatTensor(features[:79])

            # 提取我方动作（my_team=1）
            action = episode['team1_action']
            action_type = action['action_type']
            base_id = action['base_id']
            unit_type = action['unit_type']

            # 保留所有动作（包括防御）
            states.append(state_tensor)
            action_types.append(action_type)
            base_ids.append(base_id)
            unit_types.append(unit_type)

        print(f" Parsed {len(states)} samples before resampling.")
        
        # 基于折扣累积奖励进行重采样
        states, action_types, base_ids, unit_types, rewards = sample_by_rewards(
            states, action_types, base_ids, unit_types, rewards, power=SAMPLING_POWER
        )

        print(f" Processed {len(states)} samples (after resampling).")

        return torch.stack(states), \
               torch.LongTensor(action_types), \
               torch.LongTensor(base_ids), \
               torch.LongTensor(unit_types)

    except Exception as e:
        print(f" Error parsing {log_path}: {e}")
        import traceback
        traceback.print_exc()
        return None

def train_with_game_log(log_file="game_log.json", epochs=200):
    # 设备选择：MPS优先，否则CPU
    if torch.backends.mps.is_available():
        device = torch.device("mps")
        print(" Using MPS (Metal GPU) acceleration!")
    else:
        device = torch.device("cpu")
        print(" Using CPU")

    # 加载数据
    data = load_or_process_data(log_file, 'cpu')
    if not data: return
    states_tensor, action_types_tensor, base_ids_tensor, unit_types_tensor = data

    num_samples = len(states_tensor)
    model = PolicyNetwork().to(device)

    states_tensor = states_tensor.to(device)
    action_types_tensor = action_types_tensor.to(device)
    base_ids_tensor = base_ids_tensor.to(device)
    unit_types_tensor = unit_types_tensor.to(device)

    if os.name != 'nt' and hasattr(torch, 'compile') and False:  # 临时禁用compile
        print(" Compiling model (Linux/Mac)...")
        try:
            model = torch.compile(model)
        except Exception as e:
            print(f" Compile failed: {e}")
    else:
        print("  Skipping torch.compile")

    # 优化器
    optimizer = optim.AdamW(model.parameters(), lr=LEARNING_RATE_MIN, weight_decay=WEIGHT_DECAY)

    steps_per_epoch = (num_samples + BATCH_SIZE - 1) // BATCH_SIZE
    scheduler = optim.lr_scheduler.OneCycleLR(
        optimizer,
        max_lr=LEARNING_RATE_MAX,
        total_steps=steps_per_epoch * epochs,
        pct_start=LR_PCT_START,
        div_factor=LR_DIV_FACTOR,
        final_div_factor=LR_FINAL_DIV_FACTOR
    )

    criterion = nn.CrossEntropyLoss()

    print(f"\n  Starting Training | Batch: {BATCH_SIZE} | Samples: {num_samples}")

    best_loss = float('inf')
    t_start = time.time()
    no_improve_count = 0  # 记录连续无改善的epoch

    for epoch in range(epochs):
        indices = torch.randperm(num_samples).to(device)
        total_loss = 0.0
        model.train()

        for i in range(0, num_samples, BATCH_SIZE):
            idx = indices[i : min(i + BATCH_SIZE, num_samples)]

            b_state = states_tensor[idx]
            b_action = action_types_tensor[idx]
            b_base = base_ids_tensor[idx]
            b_unit = unit_types_tensor[idx]

            optimizer.zero_grad(set_to_none=True)

            # 前向传播
            a_logits, b_logits, u_logits = model(b_state)
            loss_a = criterion(a_logits, b_action)
            
            spawn_mask = (b_action == 1)
            if spawn_mask.any():
                loss_b = criterion(b_logits[spawn_mask], b_base[spawn_mask])
                loss_u = criterion(u_logits[spawn_mask], b_unit[spawn_mask])
            else:
                loss_b = torch.tensor(0.0, device=device)
                loss_u = torch.tensor(0.0, device=device)
            
            loss = (LOSS_WEIGHT_ACTION * loss_a +
                    LOSS_WEIGHT_BASE * loss_b +
                    LOSS_WEIGHT_UNIT * loss_u)

            # 反向传播
            loss.backward()
            optimizer.step()

            total_loss += loss.item()

        scheduler.step()

        avg_loss = total_loss / steps_per_epoch

        if avg_loss < best_loss - 0.001:  # 有显著改善
            best_loss = avg_loss
            no_improve_count = 0
            torch.save(model.state_dict(), SCRIPT_DIR / "policy_model_best.pth")
        else:
            no_improve_count += 1

        if (epoch + 1) % 10 == 0 or epoch == 0:
            elapsed = time.time() - t_start
            speed = (epoch + 1) / elapsed
            lr = scheduler.get_last_lr()[0]
            print(f"Epoch {epoch+1}/{epochs} | Loss: {avg_loss:.4f} | Speed: {speed:.2f} it/s | LR: {lr:.6f} | Best: {best_loss:.4f}")

        # 检查早停
        if EARLY_STOP_PATIENCE > 0 and no_improve_count >= EARLY_STOP_PATIENCE:
            print(f"\n Early stopping at epoch {epoch+1} (no improvement for {no_improve_count} epochs)")
            break

    torch.save(model.state_dict(), MODEL_PATH)
    print(f" Done. Final loss: {avg_loss:.4f}, Best loss: {best_loss:.4f}")

if __name__ == "__main__":
    train_with_game_log()
