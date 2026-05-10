#include "gamewidget.h"
#include <QPainter>
#include <QDebug>
#include <QFile>
#include <QDataStream>
#include <QDir>
#include <QMessageBox>
#include <cmath>
#include <QTextStream>


GameWidget::GameWidget(QWidget *parent)
    : QWidget(parent)
    , m_timer(nullptr)
    , m_frameCount(0)
    , m_currentTileSize(0)
    , m_offsetX(0)
    , m_offsetY(0)
    , m_monsterSpeed(100.0f)
{
    setFixedSize(1000, 720);
    setWindowTitle("保卫萝卜");
    m_isPlacingTower = false;

    m_gold = 200;
    m_lives = 10;
    m_gameOver = false;
    m_gameOverShown = false;
    m_victoryShown = false;
    m_currentTowerType = TowerType::Basic;
    m_waveIntervalFrames = 30;

    // 初始化关卡总数
    m_totalLevels = 2;

    // 游戏循环定时器
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &GameWidget::updateGame);
    m_timer->start(16);

    // 加载第一关
    loadLevel(0);
}

GameWidget::~GameWidget()
{
}

QPointF GameWidget::getTileCenter(int row, int col) const
{
    int x = m_offsetX + col * m_currentTileSize + m_currentTileSize / 2;
    int y = m_offsetY + row * m_currentTileSize + m_currentTileSize / 2;
    return QPointF(x, y);
}

void GameWidget::generateWaypoints()
{
    m_waypoints.clear();

    // 1. 找到起点和终点
    int startRow = -1, startCol = -1;
    int endRow = -1, endCol = -1;
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            if (m_map[r][c] == TileType::Start) {
                startRow = r; startCol = c;
            }
            if (m_map[r][c] == TileType::End) {
                endRow = r; endCol = c;
            }
        }
    }
    if (startRow == -1 || endRow == -1) return;

    // 2. BFS 搜索路径
    struct Node { int row, col; Node *parent; };
    bool visited[ROWS][COLS];
    memset(visited, 0, sizeof(visited));
    Node nodes[ROWS][COLS];
    QVector<Node*> queue;
    Node *startNode = &nodes[startRow][startCol];
    startNode->row = startRow; startNode->col = startCol; startNode->parent = nullptr;
    visited[startRow][startCol] = true;
    queue.append(startNode);

    int dr[4] = {0, 1, 0, -1};
    int dc[4] = {1, 0, -1, 0};
    Node *endNode = nullptr;

    while (!queue.isEmpty()) {
        Node *cur = queue.takeFirst();
        if (cur->row == endRow && cur->col == endCol) {
            endNode = cur;
            break;
        }
        for (int i = 0; i < 4; ++i) {
            int nr = cur->row + dr[i];
            int nc = cur->col + dc[i];
            if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) continue;
            if (!visited[nr][nc] && (m_map[nr][nc] == TileType::Road || m_map[nr][nc] == TileType::End)) {
                visited[nr][nc] = true;
                nodes[nr][nc].row = nr;
                nodes[nr][nc].col = nc;
                nodes[nr][nc].parent = cur;
                queue.append(&nodes[nr][nc]);
            }
        }
    }

    if (!endNode) return;

    // 3. 从终点回溯到起点，得到路径点
    QVector<QPoint> path;
    Node *p = endNode;
    while (p) {
        path.prepend(QPoint(p->row, p->col));
        p = p->parent;
    }
    m_waypoints = path;
    qDebug() << "Waypoints generated dynamically, count:" << m_waypoints.size();
}

void GameWidget::addMonster(MonsterType type)
{
    if (m_waypoints.isEmpty()) return;
    Monster m;
    QPointF startPos = getTileCenter(m_waypoints[0].x(), m_waypoints[0].y());
    m.pos = startPos;
    m.currentTarget = 1;
    switch (type) {
    case MonsterType::Normal:
        m.hp = 10;
        m.maxHp = 10;
        m.speed = m_monsterSpeed;          // 默认100
        break;
    case MonsterType::Fast:
        m.hp = 5;
        m.maxHp = 5;
        m.speed = m_monsterSpeed * 1.5f;   // 150
        break;
    case MonsterType::Tank:
        m.hp = 20;
        m.maxHp = 20;
        m.speed = m_monsterSpeed * 0.6f;   // 60
        break;
    }
    m.active = true;
    m.originalSpeed = m.speed;
    m.slowRemainingFrames = 0;
    m_monsters.append(m);
}

void GameWidget::updateMonsters(float deltaTime)
{
    for (Monster &mon : m_monsters) {
        if (!mon.active) continue;
        if (mon.slowRemainingFrames > 0) {
            mon.slowRemainingFrames--;
            if (mon.slowRemainingFrames == 0) {
                mon.speed = mon.originalSpeed;
            }
        }
        if (mon.currentTarget >= m_waypoints.size()) {
            mon.active = false;
            if (!m_gameOver) {
                m_lives--;
                qDebug() << "Monster reached end, lives left:" << m_lives;
                if (m_lives <= 0 && !m_gameOverShown) {
                    m_gameOver = true;
                    m_gameOverShown = true;
                    m_timer->stop();
                    QMessageBox::information(this, "游戏结束", "GAME OVER！\n萝卜被吃掉了！");
                }
            }
            continue;
        }

        QPointF targetPos = getTileCenter(m_waypoints[mon.currentTarget].x(),
                                          m_waypoints[mon.currentTarget].y());
        QPointF dir = targetPos - mon.pos;
        float distance = std::hypot(dir.x(), dir.y());
        float step = mon.speed * deltaTime;
        if (step >= distance) {
            mon.pos = targetPos;
            mon.currentTarget++;
        } else {
            dir /= distance;
            mon.pos += dir * step;
        }
    }
}

void GameWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int maxWidth = width();
    int maxHeight = height();
    int tileByWidth = maxWidth / COLS;
    int tileByHeight = maxHeight / ROWS;
    m_currentTileSize = qMin(tileByWidth, tileByHeight);
    m_offsetX = (maxWidth - m_currentTileSize * COLS) / 2;
    m_offsetY = (maxHeight - m_currentTileSize * ROWS) / 2;

    painter.fillRect(rect(), QColor(20, 20, 20));

    // 地图网格
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            int x = m_offsetX + col * m_currentTileSize;
            int y = m_offsetY + row * m_currentTileSize;
            QColor color;
            switch (m_map[row][col]) {
            case TileType::Road:      color = QColor(100, 100, 100); break;
            case TileType::Buildable: color = QColor(50, 150, 50); break;
            case TileType::Start:     color = QColor(0, 255, 0); break;
            case TileType::End:       color = QColor(255, 0, 0); break;
            }
            painter.fillRect(x, y, m_currentTileSize-1, m_currentTileSize-1, color);
            painter.setPen(Qt::black);
            painter.drawRect(x, y, m_currentTileSize-1, m_currentTileSize-1);
        }
    }

    // UI文字
    painter.setPen(Qt::white);
    painter.drawText(10, 20, QString("Frame: %1").arg(m_frameCount));
    painter.drawText(10, 50, QString("Wave: %1 / %2").arg(m_currentWaveIndex+1).arg(m_waves.size()));
    int activeMonsters = 0;
    for (const Monster &mon : m_monsters) if (mon.active) activeMonsters++;
    painter.drawText(10, 80, QString("Monsters: %1").arg(activeMonsters));
    painter.drawText(10, 110, QString("Gold: %1").arg(m_gold));
    painter.drawText(10, 140, QString("Lives: %1").arg(m_lives));
    if (m_gameOver) {
        painter.setPen(Qt::red);
        painter.drawText(width()/2 - 50, height()/2, "GAME OVER");
    }
    QString towerTypeStr = (m_currentTowerType == TowerType::Basic) ? "Basic" : "Slow";
    painter.drawText(10, 170, QString("Tower: %1 (1/2)").arg(towerTypeStr));

    // 炮塔面板
    int panelX = width() - 220;
    int panelY = 40;
    int lineHeight = 25;
    painter.setBrush(QColor(0, 0, 0, 150));
    painter.setPen(Qt::NoPen);
    painter.drawRect(panelX - 5, panelY - 5, 210, 4 * lineHeight + 10);
    painter.setPen(Qt::white);
    painter.setBrush(Qt::NoBrush);
    painter.drawText(panelX, panelY, "=== 炮塔列表 ===");
    painter.drawText(panelX, panelY + lineHeight, "1: 基础炮塔   100金");
    painter.drawText(panelX, panelY + 2 * lineHeight, "2: 减速炮塔    80金");
    QString currentText = (m_currentTowerType == TowerType::Basic) ? "基础炮塔" : "减速炮塔";
    painter.setPen(Qt::yellow);
    painter.drawText(panelX, panelY + 3 * lineHeight, "当前: " + currentText);
    painter.setPen(Qt::white);

    // 怪物
    painter.setPen(Qt::NoPen);
    for (const Monster &mon : m_monsters) {
        if (!mon.active) continue;
        int radius = m_currentTileSize / 3;
        QColor color;
        switch (mon.type) {
        case MonsterType::Normal:
            color = QColor(255, 100, 0);   // 橙色
            break;
        case MonsterType::Fast:
            color = QColor(255, 255, 0);   // 黄色
            break;
        case MonsterType::Tank:
            color = QColor(160, 80, 0);    // 深棕色
            break;
        default:
            color = QColor(255, 100, 0);
        }
        painter.setBrush(color);
        painter.drawEllipse(mon.pos, radius, radius);
    }
    // 血条
    painter.setPen(Qt::black);
    for (const Monster &mon : m_monsters) {
        if (!mon.active) continue;
        int radius = m_currentTileSize / 3;
        int barWidth = m_currentTileSize;
        int barHeight = 6;
        int barX = mon.pos.x() - barWidth/2;
        int barY = mon.pos.y() - radius - barHeight - 2;
        painter.setBrush(Qt::white);
        painter.drawRect(barX, barY, barWidth, barHeight);
        int hpWidth = mon.hp * barWidth / mon.maxHp;
        if (hpWidth > 0) {
            painter.setBrush(Qt::red);
            painter.drawRect(barX, barY, hpWidth, barHeight);
        }
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(Qt::NoPen);

    // 炮塔
    painter.setBrush(QColor(0, 100, 200));
    painter.setPen(Qt::black);
    for (const Tower &t : m_towers) {
        int x = m_offsetX + t.col * m_currentTileSize;
        int y = m_offsetY + t.row * m_currentTileSize;
        int radius = m_currentTileSize / 3;
        painter.drawEllipse(QPoint(x + m_currentTileSize/2, y + m_currentTileSize/2), radius, radius);
    }
    painter.setPen(Qt::red);
    painter.setBrush(Qt::NoBrush);
    for (const Tower &t : m_towers) {
        QPointF center = getTileCenter(t.row, t.col);
        painter.drawEllipse(center, t.range, t.range);
    }

    // 粒子
    painter.setPen(Qt::NoPen);
    for (const Particle &p : m_particles) {
        if (!p.active) continue;
        painter.setBrush(p.color);
        painter.drawEllipse(p.pos, p.radius, p.radius);
    }

    // 子弹
    painter.setBrush(Qt::red);
    for (const Bullet &b : m_bullets) {
        if (!b.active) continue;
        painter.drawEllipse(b.currentPos, 6, 6);
    }

    // 绘制障碍物（棕色矩形）
    painter.setBrush(QColor(139, 69, 19)); // 棕色
    painter.setPen(Qt::black);
    for (const Obstacle &obs : m_obstacles) {
        if (!obs.active) continue;
        int x = m_offsetX + obs.col * m_currentTileSize;
        int y = m_offsetY + obs.row * m_currentTileSize;
        painter.fillRect(x, y, m_currentTileSize-1, m_currentTileSize-1, QColor(139,69,19));
        painter.drawRect(x, y, m_currentTileSize-1, m_currentTileSize-1);
        // 可选：绘制血条
        int barHeight = 4;
      int hpWidth = obs.hp * m_currentTileSize / obs.maxHp;
        painter.fillRect(x, y - barHeight, hpWidth, barHeight, Qt::green);
    }

    for (const Obstacle &obs : m_obstacles) {
        qDebug() << "Obstacle at (" << obs.row << "," << obs.col << ") hp=" << obs.hp;
    }
}

void GameWidget::updateGame()
{
    static const float FIXED_DELTA = 1.0f / 60.0f;
    updateMonsters(FIXED_DELTA);
    updateBullets();
    updateTowers();
    updateParticles();
    m_frameCount++;
    updateWaveSystem();
    update();
}

void GameWidget::startWave(int waveIndex)
{
    if (waveIndex >= m_waves.size()) {
        qDebug() << "All waves completed! Victory!";
        return;
    }
    Wave &wave = m_waves[waveIndex];
    wave.active = true;
    wave.spawnedCount = 0;
    wave.nextSpawnFrame = m_frameCount + 1;
    m_currentWaveIndex = waveIndex;
    qDebug() << "Started wave" << waveIndex+1 << "total monsters:" << wave.monsterTypes.size();
}


void GameWidget::updateWaveSystem()
{
    if (m_gameOver) return;
    if (m_waveCooldown > 0) {
        m_waveCooldown--;
        return;
    }
    if (m_currentWaveIndex < m_waves.size() && !m_waves[m_currentWaveIndex].active) {
        startWave(m_currentWaveIndex);
        return;
    }
    if (m_currentWaveIndex < m_waves.size() && m_waves[m_currentWaveIndex].active) {
        Wave &wave = m_waves[m_currentWaveIndex];
        // 生成怪物
        if (wave.spawnedCount < wave.monsterTypes.size()) {
            if (m_frameCount >= wave.nextSpawnFrame) {
                MonsterType type = wave.monsterTypes[wave.spawnedCount];
                addMonster(type);
                wave.spawnedCount++;
                if (wave.spawnedCount < wave.monsterTypes.size()) {
                    wave.nextSpawnFrame = m_frameCount + wave.intervalFrames;
                }
                qDebug() << "Wave" << m_currentWaveIndex+1 << "spawned:" << wave.spawnedCount << "/" << wave.monsterTypes.size();
            }
        }
        // 检查波次是否结束（所有怪物已生成且全部死亡）
        if (wave.spawnedCount >= wave.monsterTypes.size()) {
            bool allDead = true;
            for (const Monster &mon : m_monsters) {
                if (mon.active) { allDead = false; break; }
            }
            if (allDead) {
                wave.active = false;
                m_currentWaveIndex++;
                m_waveCooldown = m_waveIntervalFrames;
                qDebug() << "Wave finished, next wave index=" << m_currentWaveIndex << " cooldown=" << m_waveCooldown;
                // 胜利判定（所有波次完成）
                if (m_currentWaveIndex >= m_waves.size() && !m_victoryShown && !m_gameOver) {
                    m_victoryShown = true;
                    m_timer->stop();
                    int ret = QMessageBox::question(this, "胜利", "恭喜通关！是否进入下一关？",
                                                    QMessageBox::Yes | QMessageBox::No);
                    if (ret == QMessageBox::Yes) {
                        if (m_currentLevel + 1 < m_totalLevels) {
                            loadLevel(m_currentLevel + 1);
                            m_victoryShown = false; // 重置胜利标志
                        } else {
                            QMessageBox::information(this, "完成", "您已完成所有关卡！");
                        }
                    }
                }
            }
        }
    }
}

bool GameWidget::windowPosToTile(const QPoint& pos, int& row, int& col)
{
    int mapX = pos.x() - m_offsetX;
    int mapY = pos.y() - m_offsetY;
    if (mapX < 0 || mapY < 0) return false;
    col = mapX / m_currentTileSize;
    row = mapY / m_currentTileSize;
    if (row >= ROWS || col >= COLS) return false;
    return true;
}

bool GameWidget::isTowerAt(int row, int col) const
{
    for (const Tower &t : m_towers) {
        if (t.row == row && t.col == col) return true;
    }
    return false;
}

void GameWidget::tryPlaceTower(int row, int col)
{
    if (isTowerAt(row, col)) return;
    if (m_map[row][col] != TileType::Buildable) return;

    Tower tower;
    tower.row = row;
    tower.col = col;
    tower.type = m_currentTowerType;
    tower.cooldown = 0;

    if (m_currentTowerType == TowerType::Basic) {
        tower.damage = 3;
        tower.range = m_currentTileSize * 2.5f;
        tower.attackCooldownMax = 20;
        const int cost = 100;
        if (m_gold < cost) return;
        m_gold -= cost;
    } else { // Slow
        tower.damage = 0;
        tower.range = m_currentTileSize * 2.5f;
        tower.attackCooldownMax = 40;
        const int cost = 80;
        if (m_gold < cost) return;
        m_gold -= cost;
    }
    m_towers.append(tower);
    qDebug() << "Tower placed at" << row << col << "type:" << (int)m_currentTowerType;
}

void GameWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int row, col;
        if (windowPosToTile(event->pos(), row, col)) {
            tryPlaceTower(row, col);
        }
    }
    QWidget::mousePressEvent(event);
}

void GameWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_1) {
        setCurrentTowerType(TowerType::Basic);
    } else if (event->key() == Qt::Key_2) {
        setCurrentTowerType(TowerType::Slow);
    } else if (event->key() == Qt::Key_S) {
        saveGame();
    } else if (event->key() == Qt::Key_L) {
        loadGame();
    } else {
        QWidget::keyPressEvent(event);
    }
}
void GameWidget::updateTowers()
{
    if (m_gameOver) return;

    for (Tower &t : m_towers) {
        if (t.cooldown > 0) {
            t.cooldown--;
            continue;
        }

        QPointF towerCenter = getTileCenter(t.row, t.col);
        bool attacked = false;

        // 优先攻击怪物
        for (Monster &mon : m_monsters) {
            if (!mon.active) continue;
            float dx = towerCenter.x() - mon.pos.x();
            float dy = towerCenter.y() - mon.pos.y();
            float dist = std::hypot(dx, dy);
            if (dist <= t.range) {
                if (t.type == TowerType::Basic) {
                    Bullet bullet;
                    bullet.startPos = towerCenter;
                    bullet.targetPos = mon.pos;
                    bullet.currentPos = towerCenter;
                    bullet.progress = 0.0f;
                    bullet.speed = 0.05f;
                    bullet.damage = t.damage;
                    bullet.active = true;
                    int idx = getFreeBulletIndex();
                    if (idx >= 0)
                        m_bullets[idx] = bullet;
                    else
                        m_bullets.append(bullet);
                } else if (t.type == TowerType::Slow) {
                    if (mon.slowRemainingFrames <= 0)
                        mon.originalSpeed = mon.speed;
                    mon.speed = mon.originalSpeed * 0.5f;
                    mon.slowRemainingFrames = 60;
                }
                attacked = true;
                break; // 每塔每帧只攻击一次
            }
        }

        // 如果没有攻击怪物，则尝试攻击障碍物
        if (!attacked) {
            for (Obstacle &obs : m_obstacles) {
                if (!obs.active) continue;
                QPointF obsCenter = getTileCenter(obs.row, obs.col);
                float dx = towerCenter.x() - obsCenter.x();
                float dy = towerCenter.y() - obsCenter.y();
                float dist = std::hypot(dx, dy);
                if (dist <= t.range) {
                    // 障碍物受到伤害
                    obs.hp -= t.damage;
                    if (obs.hp <= 0) {
                        obs.active = false;
                        m_gold += obs.reward;
                        qDebug() << "Obstacle destroyed! +" << obs.reward << "gold";
                    }
                    attacked = true;
                    break;
                }
            }
        }

        // 如果攻击了（无论是怪物还是障碍物），则设置冷却
        if (attacked) {
            t.cooldown = t.attackCooldownMax;
        }
    }
}

void GameWidget::setCurrentTowerType(TowerType type)
{
    m_currentTowerType = type;
    qDebug() << "Current tower type changed to" << (type == TowerType::Basic ? "Basic" : "Slow");
}
void GameWidget::updateParticles()
{
    for (Particle &p : m_particles) {
        if (!p.active) continue;
        p.pos += p.vel;
        p.life--;
        if (p.life <= 0)
            p.active = false;   // 改为 false，标记为不活跃
    }
}
int GameWidget::getFreeBulletIndex()
{
    for (int i = 0; i < m_bullets.size(); ++i) {
        if (!m_bullets[i].active) return i;
    }
    return -1;
}

int GameWidget::getFreeParticleIndex()
{
    for (int i = 0; i < m_particles.size(); ++i) {
        if (!m_particles[i].active) return i;
    }
    return -1;
}

void GameWidget::updateBullets()
{
    for (int i = 0; i < m_bullets.size(); ++i) {
        Bullet &b = m_bullets[i];
        if (!b.active) continue;

        b.progress += b.speed;
        if (b.progress >= 1.0f) {
            b.progress = 1.0f;
            b.currentPos = b.targetPos;
        } else {
            b.currentPos = b.startPos + (b.targetPos - b.startPos) * b.progress;
        }

        bool hit = false;
        float hitThreshold = m_currentTileSize * 0.8f;
        for (Monster &mon : m_monsters) {
            if (!mon.active) continue;
            if (QLineF(b.currentPos, mon.pos).length() < hitThreshold) {
                mon.hp -= b.damage;
                qDebug() << "Bullet hit! Monster HP:" << mon.hp;
                if (mon.hp <= 0) {
                    mon.active = false;
                    m_gold += 50;
                    for (int j = 0; j < 20; ++j) {
                        Particle p;
                        p.pos = mon.pos;
                        float angle = (rand() % 360) * 3.14159f / 180.0f;
                        float speed = 2 + rand() % 5;
                        p.vel = QPointF(cos(angle) * speed, sin(angle) * speed);
                        p.radius = 2 + rand() % 4;
                        p.life = 20 + rand() % 20;
                        p.color = QColor(255, 100 + rand() % 155, 0);
                        p.active = true;
                        int idx = getFreeParticleIndex();
                        if (idx >= 0)
                            m_particles[idx] = p;
                        else
                            m_particles.append(p);
                    }
                    qDebug() << "Monster killed! Gold:" << m_gold;
                }
                hit = true;
                break;
            }
        }

        if (hit || b.progress >= 1.0f) {
            b.active = false;
        }
    }
}

void GameWidget::saveGame()
{
    QFile file("save.dat");
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Cannot save game!";
        return;
    }
    QDataStream out(&file);
    out << m_gold << m_lives << m_currentWaveIndex;
    out << m_towers.size();
    for (const Tower &t : m_towers) {
        out << t.row << t.col << (int)t.type;
    }
    qDebug() << "Game saved!";
}

void GameWidget::loadGame()
{
    QFile file("save.dat");
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No save file found!";
        return;
    }

    resetGame();

    // 确保格子大小有效（强制触发一次计算）
    int w = width(), h = height();
    int tw = w / COLS, th = h / ROWS;
    m_currentTileSize = qMin(tw, th);
    m_offsetX = (w - m_currentTileSize * COLS) / 2;
    m_offsetY = (h - m_currentTileSize * ROWS) / 2;

    QDataStream in(&file);
    in >> m_gold >> m_lives >> m_currentWaveIndex;
    int towerCount;
    in >> towerCount;
    for (int i = 0; i < towerCount; ++i) {
        int row, col, typeInt;
        in >> row >> col >> typeInt;
        TowerType type = static_cast<TowerType>(typeInt);
        Tower t;
        t.row = row;
        t.col = col;
        t.type = type;
        t.cooldown = 0;
        if (type == TowerType::Basic) {
            t.damage = 3;
            t.range = m_currentTileSize * 2.5f;
            t.attackCooldownMax = 20;
        } else {
            t.damage = 0;
            t.range = m_currentTileSize * 2.5f;
            t.attackCooldownMax = 40;
        }
        m_towers.append(t);
    }

    for (int i = 0; i < m_waves.size(); ++i) {
        if (i < m_currentWaveIndex) {
            m_waves[i].active = false;
            // 已完成的波次 spawnedCount 设置为怪物总数
            m_waves[i].spawnedCount = m_waves[i].monsterTypes.size();
        } else if (i == m_currentWaveIndex) {
            m_waves[i].active = true;
            m_waves[i].spawnedCount = 0;
            // 下一帧立即开始生成
            m_waves[i].nextSpawnFrame = m_frameCount + 1;
        } else {
            m_waves[i].active = false;
            m_waves[i].spawnedCount = 0;
        }
    }

    if (m_currentWaveIndex >= m_waves.size()) {
        m_victoryShown = true;
        m_timer->stop();
        QMessageBox::information(this, "胜利", "恭喜通关！");
    }

    qDebug() << "Game loaded!";
}

void GameWidget::resetGame()
{
    m_towers.clear();
    m_monsters.clear();
    m_bullets.clear();
    m_particles.clear();

    m_gold = 200;
    m_lives = 10;
    m_gameOver = false;
    m_gameOverShown = false;
    m_victoryShown = false;
    m_currentTowerType = TowerType::Basic;
    m_frameCount = 0;

    for (int i = 0; i < m_waves.size(); ++i) {
        m_waves[i].active = false;
        m_waves[i].spawnedCount = 0;
        m_waves[i].nextSpawnFrame = 0;
    }
    m_currentWaveIndex = 0;
    m_waveCooldown = 0;

    if (!m_timer->isActive())
        m_timer->start(16);
    m_obstacles.clear();
}

void GameWidget::setupLevel1()
{
    // ----- 地图初始化（第一关的原始路径）-----
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
            m_map[row][col] = TileType::Buildable;

    m_map[0][0] = TileType::Start;
    for (int c = 1; c <= 10; ++c) m_map[0][c] = TileType::Road;
    for (int r = 1; r <= 10; ++r) m_map[r][10] = TileType::Road;
    for (int c = 11; c <= 19; ++c) m_map[10][c] = TileType::Road;
    m_map[10][19] = TileType::End;

    // ----- 波次配置 -----
    m_waves.clear();

    Wave wave1;
    for (int i = 0; i < 5; ++i) wave1.monsterTypes.append(MonsterType::Normal);
    wave1.spawnedCount = 0;
    wave1.intervalFrames = 45;
    wave1.nextSpawnFrame = 0;
    wave1.active = false;

    Wave wave2;
    for (int i = 0; i < 8; ++i) wave2.monsterTypes.append(MonsterType::Fast);
    wave2.spawnedCount = 0;
    wave2.intervalFrames = 45;
    wave2.nextSpawnFrame = 0;
    wave2.active = false;

    Wave wave3;
    for (int i = 0; i < 12; ++i) wave3.monsterTypes.append(MonsterType::Tank);
    wave3.spawnedCount = 0;
    wave3.intervalFrames = 45;
    wave3.nextSpawnFrame = 0;
    wave3.active = false;

    m_waves.append(wave1);
    m_waves.append(wave2);
    m_waves.append(wave3);

    m_obstacles.clear();
    // 障碍物数据：行, 列, 初始HP, 满血, 奖励
    QVector<std::tuple<int,int,int,int,int>> obsList = {
        {5,5, 15,15, 50},
        {5,6, 20,20, 60},
        {5,7, 10,10, 30},
        {6,5, 25,25, 80},
        {6,6, 15,15, 50},
        // 注意：第七行8列如果本来不该有障碍物，就不要出现在这里
    };
    for (auto &[row,col,hp,maxHp,reward] : obsList) {
        if (row>=0 && row<ROWS && col>=0 && col<COLS && m_map[row][col]==TileType::Buildable) {
            Obstacle obs;
            obs.row = row; obs.col = col;
            obs.hp = hp; obs.maxHp = maxHp;
            obs.reward = reward; obs.active = true;
            m_obstacles.append(obs);
        }
    }

    m_gold = 200;
}
void GameWidget::setupLevel2()
{
    // ----- 地图初始化（第二关 L 形路径）-----
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
            m_map[row][col] = TileType::Buildable;

    m_map[0][0] = TileType::Start;
    // 第一行从第1列到第19列设为道路
    for (int c = 1; c <= 19; ++c) m_map[0][c] = TileType::Road;
    // 最后一列从第1行到第19行设为道路
    for (int r = 1; r <= 19; ++r) m_map[r][19] = TileType::Road;
    // 终点设在(19,19)
    m_map[19][19] = TileType::End;

    // ----- 波次配置 -----
    m_waves.clear();

    // 第一波：5普通 + 5快速
    Wave wave1;
    for (int i = 0; i < 5; ++i) wave1.monsterTypes.append(MonsterType::Normal);
    for (int i = 0; i < 5; ++i) wave1.monsterTypes.append(MonsterType::Fast);
    wave1.spawnedCount = 0;
    wave1.intervalFrames = 40;
    wave1.nextSpawnFrame = 0;
    wave1.active = false;

    // 第二波：5普通 + 5坦克
    Wave wave2;
    for (int i = 0; i < 5; ++i) wave2.monsterTypes.append(MonsterType::Normal);
    for (int i = 0; i < 5; ++i) wave2.monsterTypes.append(MonsterType::Tank);
    wave2.spawnedCount = 0;
    wave2.intervalFrames = 35;
    wave2.nextSpawnFrame = 0;
    wave2.active = false;

    // 第三波：5普通 + 5快速 + 5坦克
    Wave wave3;
    for (int i = 0; i < 5; ++i) wave3.monsterTypes.append(MonsterType::Normal);
    for (int i = 0; i < 5; ++i) wave3.monsterTypes.append(MonsterType::Fast);
    for (int i = 0; i < 5; ++i) wave3.monsterTypes.append(MonsterType::Tank);
    wave3.spawnedCount = 0;
    wave3.intervalFrames = 30;
    wave3.nextSpawnFrame = 0;
    wave3.active = false;

    m_waves.append(wave1);
    m_waves.append(wave2);
    m_waves.append(wave3);

    m_gold = 150;
}
void GameWidget::loadLevel(int levelIndex)
{
    resetGame();   // 清空炮塔、怪物等，重置波次运行状态

    if (levelIndex == 0) setupLevel1();
    else if (levelIndex == 1) setupLevel2();

    m_currentLevel = levelIndex;
    m_currentWaveIndex = 0;    // 确保从第一波开始
    m_waveCooldown = 0;

    generateWaypoints();
    if (!m_timer->isActive()) m_timer->start(16);
}

void GameWidget::loadObstaclesFromFile(const QString &filename)
{
    qDebug() << "Current working directory:" << QDir::currentPath();
    qDebug() << "Absolute path:" << QFileInfo(filename).absoluteFilePath();
    qDebug() << "Attempting to open:" << QFileInfo(filename).absoluteFilePath();
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open obstacle file:" << filename;
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#")) continue; // 支持注释行

        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 4) {
            qDebug() << "Invalid line:" << line;
            continue;
        }

        int row = parts[0].toInt();
        int col = parts[1].toInt();
        int hp = parts[2].toInt();
        int reward = parts[3].toInt();

        // 检查坐标合法性以及格子是否为可建造区域
        if (row >= 0 && row < ROWS && col >= 0 && col < COLS &&
            m_map[row][col] == TileType::Buildable) {
            Obstacle obs;
            obs.row = row;
            obs.col = col;
            obs.hp = hp;
            obs.reward = reward;
            obs.active = true;
            m_obstacles.append(obs);
        } else {
            qDebug() << "Skipping obstacle at (" << row << "," << col << ") - not buildable or out of range";
        }
    }
    qDebug() << "Loaded" << m_obstacles.size() << "obstacles from" << filename;
}