#include "../include/View.h"
#include <sstream>
#include <iomanip>
#include <iostream>

GameView::GameView(std::shared_ptr<GameModel> model, std::shared_ptr<GameController> controller)
    : model(model), controller(controller),
      window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Strategy Game"),
      fontLoaded(false), texturesLoaded(false), selectedBaseIndex(0) {
    
    window.setFramerateLimit(60);

    fontLoaded = font.loadFromFile("/System/Library/Fonts/Helvetica.ttc");
    
    // 加载图片资源
    loadTextures();
}

void GameView::handleEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window.close();
        } else if (event.type == sf::Event::MouseButtonPressed) {
            if (event.mouseButton.button == sf::Mouse::Left) {
                handleMouseClick(event.mouseButton.x, event.mouseButton.y);
            }
        } else if (event.type == sf::Event::KeyPressed) {
            // 数字键1-3切换选中的基地
            if (event.key.code == sf::Keyboard::Num1) {
                selectedBaseIndex = 0;
            } else if (event.key.code == sf::Keyboard::Num2) {
                selectedBaseIndex = 1;
            } else if (event.key.code == sf::Keyboard::Num3) {
                selectedBaseIndex = 2;
            }
            // 快捷键购买士兵
            else if (event.key.code == sf::Keyboard::A) {
                handlePurchaseClick(SoldierType::ARCHER);
            } else if (event.key.code == sf::Keyboard::S) {
                handlePurchaseClick(SoldierType::INFANTRY);
            } else if (event.key.code == sf::Keyboard::D) {
                handlePurchaseClick(SoldierType::CAVALRY);
            } else if (event.key.code == sf::Keyboard::F) {
                handlePurchaseClick(SoldierType::CASTER);
            } else if (event.key.code == sf::Keyboard::G) {
                handlePurchaseClick(SoldierType::DOCTOR);
            }
        }
    }
}

void GameView::loadTextures() {
    std::string basePath = "image/";
    
    texturesLoaded = true;
    
    if (!texBaseBlue.loadFromFile(basePath + "home_blue.png")) {
        texturesLoaded = false;
    }
    if (!texBaseRed.loadFromFile(basePath + "home_red.png")) {
        texturesLoaded = false;
    }
    if (!texArcherBlue.loadFromFile(basePath + "Archer_blue.png")) {
        texturesLoaded = false;
    }
    if (!texArcherRed.loadFromFile(basePath + "Archer_red.png")) {
        texturesLoaded = false;
    }
    if (!texSaberBlue.loadFromFile(basePath + "Saber_blue.png")) {
        texturesLoaded = false;
    }
    if (!texSaberRed.loadFromFile(basePath + "Saber_red.png")) {
        texturesLoaded = false;
    }
    if (!texRiderBlue.loadFromFile(basePath + "Rider_blue.png")) {
        texturesLoaded = false;
    }
    if (!texRiderRed.loadFromFile(basePath + "Rider_red.png")) {
        texturesLoaded = false;
    }
    if (!texCasterBlue.loadFromFile(basePath + "caster_blue.png")) {
        texturesLoaded = false;
    }
    if (!texCasterRed.loadFromFile(basePath + "caster_red.png")) {
        texturesLoaded = false;
    }
    if (!texDoctorBlue.loadFromFile(basePath + "doctor_blue.png")) {
        texturesLoaded = false;
    }
    if (!texDoctorRed.loadFromFile(basePath + "doctor_red.png")) {
        texturesLoaded = false;
    }
    
    if (texturesLoaded) {
        std::cout << "All textures loaded successfully!" << std::endl;
    }
}

void GameView::render() {
    window.clear(sf::Color::Black);
    
    // 渲染各层
    renderMap();
    renderBases();
    renderSoldiers();
    renderUI();
    renderPurchasePanel();
    
    window.display();
}

void GameView::renderMap() {
    GameMap* map = model->getMap();
    
    for (int x = 0; x < MAP_SIZE; x++) {
        for (int y = 0; y < MAP_SIZE; y++) {
            Position pos(x, y);
            TerrainType terrain = map->getTerrainAt(pos);
            renderTerrain(x, y, terrain);
        }
    }
}

void GameView::renderTerrain(int x, int y, TerrainType type) {
    sf::RectangleShape cell(sf::Vector2f(CELL_SIZE, CELL_SIZE));
    cell.setPosition(gridToScreen(Position(x, y)));
    cell.setFillColor(getTerrainColor(type));
    cell.setOutlineColor(sf::Color(50, 50, 50));
    cell.setOutlineThickness(-0.5f);
    
    window.draw(cell);
}

void GameView::renderSoldiers() {
    auto soldiers = model->getSoldiers();
    
    for (const auto& soldier : soldiers) {
        if (soldier->isAlive()) {
            renderSoldier(*soldier);
        }
    }
}

void GameView::renderSoldier(const Soldier& soldier) {
    Position pos = soldier.getPosition();
    sf::Vector2f screenPos = gridToScreen(pos);
    
    if (texturesLoaded) {
        // 使用图片渲染
        sf::Sprite sprite;
        
        // 根据士兵类型和队伍选择贴图
        switch (soldier.getType()) {
            case SoldierType::ARCHER:
                sprite.setTexture(soldier.getTeam() == Team::TEAM_A ? texArcherBlue : texArcherRed);
                break;
            case SoldierType::INFANTRY:
                sprite.setTexture(soldier.getTeam() == Team::TEAM_A ? texSaberBlue : texSaberRed);
                break;
            case SoldierType::CAVALRY:
                sprite.setTexture(soldier.getTeam() == Team::TEAM_A ? texRiderBlue : texRiderRed);
                break;
            case SoldierType::CASTER:
                sprite.setTexture(soldier.getTeam() == Team::TEAM_A ? texCasterBlue : texCasterRed);
                break;
            case SoldierType::DOCTOR:
                sprite.setTexture(soldier.getTeam() == Team::TEAM_A ? texDoctorBlue : texDoctorRed);
                break;
        }
        
        // 缩放图片以适应格子大小
        sf::Vector2u texSize = sprite.getTexture()->getSize();
        float scaleX = CELL_SIZE / texSize.x;
        float scaleY = CELL_SIZE / texSize.y;
        sprite.setScale(scaleX, scaleY);
        sprite.setPosition(screenPos);
        
        // 根据HP调整透明度
        float hpRatio = static_cast<float>(soldier.getHp()) / soldier.getMaxHp();
        sf::Color color = sf::Color::White;
        color.a = static_cast<sf::Uint8>(255 * (0.3f + 0.7f * hpRatio));
        sprite.setColor(color);
        
        window.draw(sprite);
    }
}

void GameView::renderBases() {
    // 渲染Team A的所有基地
    for (const auto& base : model->getBasesTeamA()) {
        renderBase(base.get());
    }
    
    // 渲染Team B的所有基地
    for (const auto& base : model->getBasesTeamB()) {
        renderBase(base.get());
    }
}

void GameView::renderBase(const Base* base) {
    if (!base->isAlive()) return;
    
    Position pos = base->getPosition();
    sf::Vector2f screenPos = gridToScreen(pos);
    
    if (texturesLoaded) {
        // 使用图片渲染基地
        sf::Sprite sprite;
        sprite.setTexture(base->getTeam() == Team::TEAM_A ? texBaseBlue : texBaseRed);
        
        // 缩放图片以适应格子大小
        sf::Vector2u texSize = sprite.getTexture()->getSize();
        float scaleX = CELL_SIZE / texSize.x;
        float scaleY = CELL_SIZE / texSize.y;
        sprite.setScale(scaleX, scaleY);
        sprite.setPosition(screenPos);
        
        window.draw(sprite);
    }
    
    // 绘制HP条
    float hpRatio = static_cast<float>(base->getHp()) / base->getMaxHp();
    sf::RectangleShape hpBar(sf::Vector2f(CELL_SIZE * hpRatio, CELL_SIZE * 0.1f));
    hpBar.setPosition(screenPos.x, screenPos.y - CELL_SIZE * 0.15f);
    hpBar.setFillColor(sf::Color::Green);
    window.draw(hpBar);
    
    sf::RectangleShape hpBackground(sf::Vector2f(CELL_SIZE, CELL_SIZE * 0.1f));
    hpBackground.setPosition(screenPos.x, screenPos.y - CELL_SIZE * 0.15f);
    hpBackground.setFillColor(sf::Color(50, 50, 50));
    hpBackground.setOutlineColor(sf::Color::White);
    hpBackground.setOutlineThickness(-1.0f);
    window.draw(hpBackground);
}

void GameView::renderUI() {
    if (!fontLoaded) return;
    
    // 显示回合数
    sf::Text turnText;
    turnText.setFont(font);
    turnText.setCharacterSize(20);
    turnText.setFillColor(sf::Color::White);
    std::ostringstream oss;
    oss << "Turn: " << model->getTurnCount();
    turnText.setString(oss.str());
    turnText.setPosition(10, 10);
    window.draw(turnText);
    
    // 显示士兵数量
    auto soldiers = model->getSoldiers();
    int teamACount = 0, teamBCount = 0;
    for (const auto& s : soldiers) {
        if (s->isAlive()) {
            if (s->getTeam() == Team::TEAM_A) teamACount++;
            else teamBCount++;
        }
    }
    
    sf::Text soldierText;
    soldierText.setFont(font);
    soldierText.setCharacterSize(20);
    soldierText.setFillColor(sf::Color::White);
    oss.str("");
    oss << "Team A: " << teamACount << " | Team B: " << teamBCount;
    soldierText.setString(oss.str());
    soldierText.setPosition(10, 40);
    window.draw(soldierText);
    
    // 显示基地HP（所有基地的总HP）
    int teamATotalHp = 0, teamBTotalHp = 0;
    for (const auto& base : model->getBasesTeamA()) {
        teamATotalHp += base->getHp();
    }
    for (const auto& base : model->getBasesTeamB()) {
        teamBTotalHp += base->getHp();
    }
    
    sf::Text baseHpText;
    baseHpText.setFont(font);
    baseHpText.setCharacterSize(20);
    baseHpText.setFillColor(sf::Color::White);
    oss.str("");
    oss << "Base A HP: " << teamATotalHp << " | Base B HP: " << teamBTotalHp;
    baseHpText.setString(oss.str());
    baseHpText.setPosition(10, 70);
    window.draw(baseHpText);
    
    // 如果游戏结束，显示胜利者
    if (model->isGameOver()) {
        sf::Text gameOverText;
        gameOverText.setFont(font);
        gameOverText.setCharacterSize(50);
        gameOverText.setFillColor(sf::Color::Yellow);
        gameOverText.setStyle(sf::Text::Bold);
        
        std::string winner = (model->getWinner() == Team::TEAM_A) ? "Team A" : "Team B";
        gameOverText.setString(winner + " Wins!");
        
        sf::FloatRect bounds = gameOverText.getLocalBounds();
        gameOverText.setPosition(
            (WINDOW_WIDTH - bounds.width) / 2,
            (WINDOW_HEIGHT - bounds.height) / 2
        );
        
        window.draw(gameOverText);
    }
}

sf::Color GameView::getTerrainColor(TerrainType type) {
    switch (type) {
        case TerrainType::PLAIN:
            return sf::Color(180, 180, 180);  // 灰色
        case TerrainType::MOUNTAIN:
            return sf::Color(139, 137, 137);  // 暗灰色
        case TerrainType::RIVER:
            return sf::Color(135, 206, 250);  // 浅蓝色
        case TerrainType::BASE_A:
            return sf::Color(100, 149, 237);  // 蓝色
        case TerrainType::BASE_B:
            return sf::Color(220, 20, 60);    // 红色
        default:
            return sf::Color::Black;
    }
}

sf::Color GameView::getTeamColor(Team team) {
    if (team == Team::TEAM_A) {
        return sf::Color(100, 149, 237);  // 蓝色
    } else {
        return sf::Color(220, 20, 60);    // 红色
    }
}

sf::Color GameView::getSoldierColor(const Soldier& soldier) {
    sf::Color baseColor = getTeamColor(soldier.getTeam());
    
    // 根据HP调整亮度
    float hpRatio = static_cast<float>(soldier.getHp()) / soldier.getMaxHp();
    
    return sf::Color(
        static_cast<sf::Uint8>(baseColor.r * hpRatio),
        static_cast<sf::Uint8>(baseColor.g * hpRatio),
        static_cast<sf::Uint8>(baseColor.b * hpRatio)
    );
}

sf::Vector2f GameView::gridToScreen(const Position& pos) {
    return sf::Vector2f(
        pos.x * CELL_SIZE,
        pos.y * CELL_SIZE
    );
}

void GameView::renderPurchasePanel() {
    if (!fontLoaded) return;
    
    // ========== Team B 信息面板 (右上角) - AI模型 ==========
    sf::RectangleShape panelA(sf::Vector2f(280, 180));
    panelA.setPosition(WINDOW_WIDTH - 290, 10);
    panelA.setFillColor(sf::Color(80, 20, 20, 220));  // 红色调
    panelA.setOutlineColor(sf::Color(255, 100, 100));
    panelA.setOutlineThickness(2);
    window.draw(panelA);
    
    // Team B 标题
    sf::Text titleA("=== AI MODEL (Red) ===", font, 16);
    titleA.setPosition(WINDOW_WIDTH - 280, 20);
    titleA.setFillColor(sf::Color(255, 150, 150));
    titleA.setStyle(sf::Text::Bold);
    window.draw(titleA);
    
    // Team B 能量
    int energyA = model->getEnergy(Team::TEAM_B);
    sf::Text energyTextA("Energy: " + std::to_string(energyA), font, 14);
    energyTextA.setPosition(WINDOW_WIDTH - 280, 50);
    energyTextA.setFillColor(sf::Color::Yellow);
    window.draw(energyTextA);
    
    // Team B 基地数量和总HP
    int baseCountA = 0, totalHpA = 0;
    for (const auto& base : model->bases) {
        if (base->getTeam() == Team::TEAM_B) {
            baseCountA++;
            totalHpA += base->getHp();
        }
    }
    sf::Text baseTextA("Bases: " + std::to_string(baseCountA) + " | HP: " + std::to_string(totalHpA), font, 13);
    baseTextA.setPosition(WINDOW_WIDTH - 280, 75);
    baseTextA.setFillColor(sf::Color::White);
    window.draw(baseTextA);
    
    // Team B 士兵统计
    int soldierCountA = 0;
    for (const auto& soldier : model->soldiers) {
        if (soldier->getTeam() == Team::TEAM_B) {
            soldierCountA++;
        }
    }
    sf::Text soldierTextA("Soldiers: " + std::to_string(soldierCountA), font, 14);
    soldierTextA.setPosition(WINDOW_WIDTH - 280, 100);
    soldierTextA.setFillColor(sf::Color::White);
    window.draw(soldierTextA);
    
    // Team B AI类型
    sf::Text aiTypeA("Type: AI Model", font, 12);
    aiTypeA.setPosition(WINDOW_WIDTH - 280, 125);
    aiTypeA.setFillColor(sf::Color(180, 180, 180));
    window.draw(aiTypeA);
    
    // ========== Team A 购买面板 (左下角) - 人类控制 ==========
    sf::RectangleShape panelB(sf::Vector2f(320, 330));
    panelB.setPosition(10, WINDOW_HEIGHT - 340);
    panelB.setFillColor(sf::Color(20, 20, 80, 220));  // 蓝色调
    panelB.setOutlineColor(sf::Color(100, 150, 255));
    panelB.setOutlineThickness(2);
    window.draw(panelB);
    
    // Team A 标题
    sf::Text titleB("=== YOUR TEAM (Blue) ===", font, 18);
    titleB.setPosition(20, WINDOW_HEIGHT - 330);
    titleB.setFillColor(sf::Color(150, 200, 255));
    titleB.setStyle(sf::Text::Bold);
    window.draw(titleB);
    
    // Team A 能量显示
    int energy = model->getEnergy(Team::TEAM_A);
    sf::Text energyText("Energy: " + std::to_string(energy), font, 16);
    energyText.setPosition(20, WINDOW_HEIGHT - 300);
    energyText.setFillColor(sf::Color::Yellow);
    window.draw(energyText);
    
    // Team A 基地选择提示
    sf::Text baseText("Base: " + std::to_string(selectedBaseIndex + 1) + " (1-3)", font, 14);
    baseText.setPosition(20, WINDOW_HEIGHT - 270);
    baseText.setFillColor(sf::Color::Cyan);
    window.draw(baseText);
    
    // 购买按钮 - Archer
    sf::RectangleShape archerBtn(sf::Vector2f(300, 30));
    archerBtn.setPosition(15, WINDOW_HEIGHT - 225);
    archerBtn.setFillColor(energy >= ARCHER_COST ? sf::Color(0, 120, 0) : sf::Color(120, 0, 0));
    archerBtn.setOutlineColor(sf::Color::White);
    archerBtn.setOutlineThickness(1);
    window.draw(archerBtn);
    
    sf::Text archerText("A - Archer (" + std::to_string(ARCHER_COST) + ")", font, 14);
    archerText.setPosition(25, WINDOW_HEIGHT - 220);
    archerText.setFillColor(sf::Color::White);
    window.draw(archerText);
    
    // 购买按钮 - Infantry
    sf::RectangleShape infantryBtn(sf::Vector2f(300, 30));
    infantryBtn.setPosition(15, WINDOW_HEIGHT - 190);
    infantryBtn.setFillColor(energy >= INFANTRY_COST ? sf::Color(0, 120, 0) : sf::Color(120, 0, 0));
    infantryBtn.setOutlineColor(sf::Color::White);
    infantryBtn.setOutlineThickness(1);
    window.draw(infantryBtn);
    
    sf::Text infantryText("S - Infantry (" + std::to_string(INFANTRY_COST) + ")", font, 14);
    infantryText.setPosition(25, WINDOW_HEIGHT - 185);
    infantryText.setFillColor(sf::Color::White);
    window.draw(infantryText);
    
    // 购买按钮 - Cavalry
    sf::RectangleShape cavalryBtn(sf::Vector2f(300, 30));
    cavalryBtn.setPosition(15, WINDOW_HEIGHT - 155);
    cavalryBtn.setFillColor(energy >= CAVALRY_COST ? sf::Color(0, 120, 0) : sf::Color(120, 0, 0));
    cavalryBtn.setOutlineColor(sf::Color::White);
    cavalryBtn.setOutlineThickness(1);
    window.draw(cavalryBtn);
    
    sf::Text cavalryText("D - Cavalry (" + std::to_string(CAVALRY_COST) + ")", font, 14);
    cavalryText.setPosition(25, WINDOW_HEIGHT - 150);
    cavalryText.setFillColor(sf::Color::White);
    window.draw(cavalryText);
    
    // 购买按钮 - Caster（法师）
    sf::RectangleShape casterBtn(sf::Vector2f(300, 30));
    casterBtn.setPosition(15, WINDOW_HEIGHT - 120);
    casterBtn.setFillColor(energy >= CASTER_COST ? sf::Color(0, 120, 0) : sf::Color(120, 0, 0));
    casterBtn.setOutlineColor(sf::Color::White);
    casterBtn.setOutlineThickness(1);
    window.draw(casterBtn);
    
    sf::Text casterText("F - Caster (" + std::to_string(CASTER_COST) + ")", font, 14);
    casterText.setPosition(25, WINDOW_HEIGHT - 115);
    casterText.setFillColor(sf::Color::White);
    window.draw(casterText);
    
    // 购买按钮 - Doctor（医疗兵）
    sf::RectangleShape doctorBtn(sf::Vector2f(300, 30));
    doctorBtn.setPosition(15, WINDOW_HEIGHT - 85);
    doctorBtn.setFillColor(energy >= DOCTOR_COST ? sf::Color(0, 120, 0) : sf::Color(120, 0, 0));
    doctorBtn.setOutlineColor(sf::Color::White);
    doctorBtn.setOutlineThickness(1);
    window.draw(doctorBtn);
    
    sf::Text doctorText("G - Doctor (" + std::to_string(DOCTOR_COST) + ")", font, 14);
    doctorText.setPosition(25, WINDOW_HEIGHT - 80);
    doctorText.setFillColor(sf::Color::White);
    window.draw(doctorText);
    
    // Team A 士兵统计
    int soldierCountA_panel = 0;
    for (const auto& soldier : model->soldiers) {
        if (soldier->getTeam() == Team::TEAM_A) {
            soldierCountA_panel++;
        }
    }
    sf::Text soldierTextB("Soldiers: " + std::to_string(soldierCountA_panel), font, 12);
    soldierTextB.setPosition(20, WINDOW_HEIGHT - 50);
    soldierTextB.setFillColor(sf::Color(200, 200, 200));
    window.draw(soldierTextB);
}

void GameView::handleMouseClick(int mouseX, int mouseY) {
    // 检查是否点击左下角购买按钮区域 (Team B)
    if (mouseX >= 15 && mouseX <= 315) {
        if (mouseY >= WINDOW_HEIGHT - 255 && mouseY <= WINDOW_HEIGHT - 225) {
            // 点击弓箭手按钮
            handlePurchaseClick(SoldierType::ARCHER);
        } else if (mouseY >= WINDOW_HEIGHT - 220 && mouseY <= WINDOW_HEIGHT - 190) {
            // 点击步兵按钮
            handlePurchaseClick(SoldierType::INFANTRY);
        } else if (mouseY >= WINDOW_HEIGHT - 185 && mouseY <= WINDOW_HEIGHT - 155) {
            // 点击骑兵按钮
            handlePurchaseClick(SoldierType::CAVALRY);
        } else if (mouseY >= WINDOW_HEIGHT - 150 && mouseY <= WINDOW_HEIGHT - 120) {
            // 点击法师按钮
            handlePurchaseClick(SoldierType::CASTER);
        } else if (mouseY >= WINDOW_HEIGHT - 115 && mouseY <= WINDOW_HEIGHT - 85) {
            // 点击医疗兵按钮
            handlePurchaseClick(SoldierType::DOCTOR);
        }
    }
}

void GameView::handlePurchaseClick(SoldierType type) {
    // 人类控制Team A（蓝色）
    const auto& bases = model->getBasesTeamA();
    if (selectedBaseIndex < 0 || selectedBaseIndex >= bases.size()) return;
    if (!bases[selectedBaseIndex]->isAlive()) return;
    
    Position basePos = bases[selectedBaseIndex]->getPosition();
    bool success = controller->purchaseSoldier(Team::TEAM_A, type, basePos);
    
    if (!success) {
        std::cout << "Purchase failed: Not enough energy or position unavailable" << std::endl;
    }
}
