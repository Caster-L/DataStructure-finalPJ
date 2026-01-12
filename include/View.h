#ifndef VIEW_H
#define VIEW_H

#include "Model.h"
#include "Controller.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>

// 游戏视图类
class GameView {
private:
    std::shared_ptr<GameModel> model;
    std::shared_ptr<GameController> controller;
    sf::RenderWindow window;
    sf::Font font;
    bool fontLoaded;
    
    // 图片资源
    sf::Texture texBaseBlue, texBaseRed;
    sf::Texture texArcherBlue, texArcherRed;
    sf::Texture texSaberBlue, texSaberRed;
    sf::Texture texRiderBlue, texRiderRed;
    sf::Texture texCasterBlue, texCasterRed;
    sf::Texture texDoctorBlue, texDoctorRed;
    bool texturesLoaded;
    
    // UI状态
    int selectedBaseIndex;  // 当前选中的基地索引（-1表示未选中）
    
public:
    GameView(std::shared_ptr<GameModel> model, std::shared_ptr<GameController> controller);
    
    bool isOpen() const { return window.isOpen(); }
    void handleEvents();
    void render();
    
private:
    // 渲染组件
    void renderMap();
    void renderTerrain(int x, int y, TerrainType type);
    void renderSoldiers();
    void renderSoldier(const Soldier& soldier);
    void renderBases();
    void renderBase(const Base* base);
    void renderUI();
    void renderPurchasePanel();  // 渲染购买面板
    
    // 事件处理
    void handleMouseClick(int mouseX, int mouseY);
    void handlePurchaseClick(SoldierType type);
    
    // 资源加载
    void loadTextures();
    
    // 颜色选择
    sf::Color getTerrainColor(TerrainType type);
    sf::Color getTeamColor(Team team);
    sf::Color getSoldierColor(const Soldier& soldier);
    
    // 辅助函数
    sf::Vector2f gridToScreen(const Position& pos);
};

#endif // VIEW_H
