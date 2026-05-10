#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QPoint>
#include <QPointF>
#include <QMouseEvent>
#include <QMessageBox>
#include <QLineF>

enum class MonsterType {
    Normal,   // 普通：HP=10, 速度=100
    Fast,     // 快速：HP=5,  速度=150
    Tank      // 坦克：HP=20, 速度=60
};

// 波次结构体
struct Wave {
    QVector<MonsterType> monsterTypes; // 按顺序的怪物类型列表
    int spawnedCount;                  // 已生成数量（用作索引）
    int intervalFrames;                // 生成间隔（帧数）
    int nextSpawnFrame;                // 下一只怪物生成的帧计数
    bool active;                       // 是否正在进行中
};
// 怪物结构体（平滑移动版）
struct Monster {
    QPointF pos;           // 当前像素坐标（窗口坐标系，格子中心点）
    int currentTarget;     // 当前要走向的路径点索引（m_waypoints中的下一个点）
    int hp;                // 生命值
    bool active;           // 是否存活（未到达终点）
    float speed;          // 移动速度（像素/秒）
    float originalSpeed;   // 原始速度
    int slowRemainingFrames; // 剩余减速帧数
    int maxHp;   // 最大生命值
    MonsterType type;
};
struct Particle {
    QPointF pos;
    QPointF vel;
    float radius;
    int life;
    QColor color;
    bool active;   // 新增
};
struct Bullet {
    QPointF startPos;      // 炮塔发射点
    QPointF targetPos;     // 发射时目标怪物位置（直线飞行）
    QPointF currentPos;    // 当前飞行位置
    float progress;        // 0→1
    float speed;           // 每帧增加的进度（0.05 约20帧到达）
    int damage;            // 伤害值
    bool active;
};
enum class TowerType {
    Basic,
    Slow
};
struct Tower {
    int row, col;           // 所在的格子坐标
    TowerType type;         // 炮塔类型
    int damage;             // 攻击力
    float range;            // 攻击范围（像素）
    int cooldown;           // 剩余冷却帧数
    int attackCooldownMax;  // 攻击间隔（帧数）
};

enum class TileType {
    Road,       // 道路（怪物行走路径）
    Buildable,  // 可建造炮塔的位置
    Start,      // 怪物起点
    End         // 终点（萝卜）
};
struct Obstacle {
    int row, col;
    int hp;
    int maxHp;   // 新增
    int reward;
    bool active;
};
class GameWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GameWidget(QWidget *parent = nullptr);
    ~GameWidget();

    // 公共方法
    bool windowPosToTile(const QPoint& pos, int& row, int& col);
    void addMonster(MonsterType type = MonsterType::Normal);

protected:
    void paintEvent(QPaintEvent *event) override;
    void updateGame();   // 游戏逻辑更新函数
    void mousePressEvent(QMouseEvent *event) override;

private:
    // 基础成员
    QTimer *m_timer;
    int m_frameCount;
    static const int ROWS = 20;
    static const int COLS = 20;
    TileType m_map[ROWS][COLS];
    int m_currentTileSize;          // 当前绘制的格子大小（像素）
    int m_offsetX, m_offsetY;       // 绘制地图时的偏移量（用于居中）

    // 怪物系统（平滑移动）
    QVector<QPoint> m_waypoints;    // 路径点（格子行列）
    QVector<Monster> m_monsters;
    float m_monsterSpeed;           // 怪物移动速度（像素/秒）

    // 波次系统
    QVector<Wave> m_waves;
    int m_currentWaveIndex;
    int m_waveIntervalFrames;       // 波次之间的等待帧数
    int m_waveCooldown;             // 波次结束后的冷却帧计数

    // 炮塔系统
    QVector<Tower> m_towers;   // 炮塔列表
    bool m_isPlacingTower;     // 是否处于放置炮塔模式（简单起见，直接点击建造）

    QPointF getTileCenter(int row, int col) const;

    QVector<Particle> m_particles;   // 粒子列表

    QVector<Bullet> m_bullets;
    QVector<Obstacle> m_obstacles;

    int m_gold;          // 当前金币
    int m_lives;         // 当前生命值（萝卜血量）
    bool m_gameOver;     // 游戏是否结束
    bool m_gameOverShown;   // 是否已弹出过游戏结束框
    bool m_victoryShown;   // 是否已弹出胜利消息框
    TowerType m_currentTowerType;   // 当前选中的炮塔类型
    int getFreeBulletIndex();     // 获取空闲子弹槽位
    int getFreeParticleIndex();   // 获取空闲粒子槽位


    // 私有函数
    void generateWaypoints();       // 生成格子路径点
    void updateMonsters(float deltaTime);   // 更新怪物平滑移动
    void startWave(int waveIndex);
    void updateWaveSystem();
    void tryPlaceTower(int row, int col);   // 尝试在指定格子放置炮塔
    void updateTowers();                     // 每帧更新炮塔攻击
    bool isTowerAt(int row, int col) const;
    void keyPressEvent(QKeyEvent *event) override;// 辅助函数：根据格子行列获取当前窗口下的格子中心像素坐标
    void setCurrentTowerType(TowerType type);
    void updateParticles();
    void updateBullets();
    void saveGame();        // 保存游戏
    void loadGame();        // 加载游戏
    void resetGame();       // 重置游戏（清空炮塔、怪物等）
    void loadObstaclesFromFile(const QString &filename);

    int m_currentLevel;
    int m_totalLevels;
    void loadLevel(int levelIndex);
    void setupLevel1();
    void setupLevel2();
};

#endif // GAMEWIDGET_H