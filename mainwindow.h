#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QTimer>
#include <QVector>
#include <QStandardItemModel>
#include <QTreeWidgetItem>

#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"

namespace Ui{
class MainWindow;
}

//地区树节点
struct Region {
    int id;
    int parentId;
    QString name;
};



class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    //构造：加载.ui界面，初始化影像显示、定时器、地区树与病人列表
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();//析构：释放ui资源

    //影像处理
    //初始化：读Tumor.jpg显示到CT_Img_Label，LCD显示年月日，显示时间
    void initMainWindow();

    // 把 myCtQImage 等比缩放后显示到 CT_Img_Label
    void ctImgShow();

    // 文件对话框选 CT 图 → imread 读入 → 判空 → 转 RGB/灰度 → 显示
    void ctImgRead();

    // 模拟耗时 + 推进度条 + 调 ctImgHoughCircles 做霍夫圆检测
    void ctImgProc();

    // 把处理结果保存为 Tumor_proced.jpg
    void ctImgSave();

    // 高斯模糊 + HoughCircles 检测圆 + circle 画圆
    void ctImgHoughCircles();

private slots:
    // 定时器回调：timeEdit 显示当前时间
    void onTimeOut();

    // 「开始诊断」按钮：读图 → 进度条 → 处理 → 保存
    void on_startPushButton_clicked();

    // ---------- 业务数据 ----------
    // 点击地区树节点：按该地区（含子科室）重新加载病人列表
    void onRegionClicked(QTreeWidgetItem *item, int column);

    // 表格当前行变化：按该行病人 id 回填右侧表单
    void onPatientRowChanged(const QModelIndex &current, const QModelIndex &previous);

    // 在当前选中地区下新增一个病人
    void addPatient();

    // 把表单内容 UPDATE 到 patient 表
    void savePatient();

    // 确认后删除当前病人（病历靠外键 ON DELETE CASCADE 级联删除）
    void deletePatient();

    // 保存病历文本 + 照片到 medical_record（无则 INSERT、有则 UPDATE）
    void saveRecord();

    // 选照片 → 统一转 JPG 字节 → 预览缩略图
    void uploadPhoto();
private:
    // 从 MySQL 加载地区树到 treeWidget
    void loadRegionTree();

    // 加载某地区（含子科室）的病人到 basicTableView
    void loadPatients(int regionId);

    // 返回某地区节点及其全部后代 id（DFS）
    QVector<int> regionAndDescendants(int regionId) const;

    // 返回当前树节点的地区 id（无选中返回 0）
    int selectedRegionId() const;

    // 返回当前表格行的病人 id（无选中返回 0）
    int currentPatientId() const;

    // 按病人 id 回填表单 + 病历 + 照片
    void fillForm(int patientId);

    // 在表格里定位并选中指定病人行
    void selectPatient(int patientId);

    // 用代码创建底部「新增/保存/删除/保存病历/上传照片」按钮并连接槽
    void buildCrudButtons();


    Ui::MainWindow *ui;

    // 影像相关成员
    cv::Mat myCtImg;       // 当前 CT 图（RGB）
    cv::Mat myCtGrayImg;   // 灰度图（霍夫圆检测用）
    QImage myCtQImage;     // 用于显示/保存的 QImage
    QTimer *myTimer;       // 每秒触发 onTimeOut 刷新时间

    // 业务相关成员
    QStandardItemModel *patientModel; // 病人列表模型
    QVector<Region> regions;          // 地区树（扁平数组，按 parentId 挂接）

    // 当前选中病人的照片（JPG 字节），保存病历/上传时一起写入 medical_record.photo
    QByteArray mPhotoBytes;
};

#endif // MAINWINDOW_H
