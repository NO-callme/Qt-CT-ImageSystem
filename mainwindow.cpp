#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "db.h"

#include <QFileDialog>
#include <QElapsedTimer>
#include <QDate>
#include <QTime>
#include <QCoreApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QHash>
#include <QStandardItem>
#include <QPixmap>
#include <QBuffer>


// 按需引入 OpenCV / 标准库符号，避免 `using namespace cv` 把
// cv::ACCESS_MASK 等带入全局命名空间、与 Windows 头文件冲突。
using cv::Mat;
using cv::imread;
using cv::cvtColor;
using cv::COLOR_BGR2RGB;
using cv::COLOR_RGB2GRAY;
using cv::COLOR_GRAY2BGR;
using cv::GaussianBlur;
using cv::HoughCircles;
using cv::HOUGH_GRADIENT;
using cv::Size;
using cv::Point;
using cv::Scalar;
using cv::Vec3f;
using cv::circle;
using std::vector;
using std::size_t;


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initMainWindow();

    // 病人列表（QStandardItemModel，数据来自 MySQL）
    patientModel = new QStandardItemModel(this);
    patientModel->setHorizontalHeaderLabels(
        {u8"姓名", u8"性别", u8"民族", u8"出生日期", u8"科室"});
    ui->basicTableView->setModel(patientModel);
    ui->basicTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->basicTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->basicTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->basicTableView->horizontalHeader()->setStretchLastSection(true);
    connect(ui->basicTableView->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this, &MainWindow::onPatientRowChanged);

    // 左侧地区树：点击节点筛选该地区（含子科室）的病人
    connect(ui->treeWidget, &QTreeWidget::itemClicked,
            this, &MainWindow::onRegionClicked);

    loadRegionTree();
    loadPatients(0);

    buildCrudButtons();

}

MainWindow::~MainWindow()
{
    delete ui;
}


// ============================================================
//  影像部分
// ============================================================

//初始化：读Tumor.jpg显示到CT_Img_Label，LCD显示年月日，显示时间
void MainWindow::initMainWindow()
{
    QString ctImgPath = "Tumor.jpg";
    Mat ctImg = imread(ctImgPath.toLatin1().data());
    if (!ctImg.empty()) {
        cvtColor(ctImg, ctImg, COLOR_BGR2RGB);
        myCtImg = ctImg;
        myCtQImage = QImage(myCtImg.data, myCtImg.cols, myCtImg.rows,
                            myCtImg.step, QImage::Format_RGB888);
    }
    ctImgShow();

    QDate date = QDate::currentDate();
    ui->yearLcdNumber->display(date.year());
    ui->monthLcdNumber->display(date.month());
    ui->dayLcdNumber->display(date.day());

    myTimer = new QTimer(this);
    myTimer->setInterval(1000);
    myTimer->start();
    connect(myTimer, SIGNAL(timeout()), this, SLOT(onTimeOut()));
}

// 把 myCtQImage 等比缩放后显示到 CT_Img_Label
void MainWindow::ctImgShow()
{
    if (myCtQImage.isNull())
        ui->CT_Img_Label->clear();
    else
        ui->CT_Img_Label->setPixmap(QPixmap::fromImage(myCtQImage.scaled(
            ui->CT_Img_Label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}

// 文件对话框选 CT 图 → imread 读入 → 判空 → 转 RGB/灰度 → 显示
void MainWindow::ctImgRead()
{
    QString ctImgName = QFileDialog::getOpenFileName(this, u8"载入CT相片", ".",
                                                     "Image File(*.png *.jpg *.jpeg *.bmp)");
    if (ctImgName.isEmpty())
        return;

    Mat ctImg = imread(ctImgName.toLatin1().data());
    if (ctImg.empty()) {
        QMessageBox::warning(this, u8"载入失败", u8"无法读取该图片，请确认文件格式正确。");
        return;
    }

    Mat ctRgbImg, ctGrayImg;
    cvtColor(ctImg, ctRgbImg, COLOR_BGR2RGB);
    cvtColor(ctRgbImg, ctGrayImg, COLOR_RGB2GRAY);
    myCtImg = ctRgbImg;
    myCtGrayImg = ctGrayImg;
    myCtQImage = QImage(myCtImg.data, myCtImg.cols, myCtImg.rows,
                        myCtImg.step, QImage::Format_RGB888);
    ctImgShow();
}

// 模拟耗时 + 推进度条 + 调 ctImgHoughCircles 做霍夫圆检测
void MainWindow::ctImgProc()
{
    QElapsedTimer time;
    time.start();

    ui->progressBar->setValue(19);
    while (time.elapsed() < 2000) QCoreApplication::processEvents();

    ctImgHoughCircles();

    while (time.elapsed() < 2000) QCoreApplication::processEvents();
    ui->progressBar->setValue(ui->progressBar->value() + 20);
    ctImgShow();

    while (time.elapsed() < 2000) QCoreApplication::processEvents();
    ui->progressBar->setValue(ui->progressBar->maximum());
    QMessageBox::information(this, u8"完毕", u8"子宫内壁见椭球形阴影，疑似子宫肌瘤");
}

// 把处理结果保存为 Tumor_proced.jpg
void MainWindow::ctImgSave()
{
    if (myCtQImage.isNull())
        return;
    if (!myCtQImage.save("Tumor_proced.jpg", "JPG"))
        QMessageBox::warning(this, u8"保存失败", u8"无法保存处理后的CT影像。");
}

// 高斯模糊 + HoughCircles 检测圆 + circle 画圆
void MainWindow::ctImgHoughCircles()
{
    if (myCtGrayImg.empty())
        return;

    Mat ctGrayImg = myCtGrayImg.clone();
    Mat ctColorImg;
    cvtColor(ctGrayImg, ctColorImg, COLOR_GRAY2BGR);
    GaussianBlur(ctGrayImg, ctGrayImg, Size(9, 9), 2, 2);

    vector<Vec3f> h_circles;
    HoughCircles(ctGrayImg, h_circles, HOUGH_GRADIENT, 2, ctGrayImg.rows / 8, 200, 100);

    int processValue = 45;
    ui->progressBar->setValue(processValue);
    QElapsedTimer time;
    time.start();
    while (time.elapsed() < 2000) QCoreApplication::processEvents();

    for (size_t i = 0; i < h_circles.size(); i++) {
        Point center(cvRound(h_circles[i][0]), cvRound(h_circles[i][1]));
        int h_radius = cvRound(h_circles[i][2]);
        circle(ctColorImg, center, h_radius, Scalar(238, 0, 238), 3, 8, 0);
        circle(ctColorImg, center, 3, Scalar(238, 0, 0), -1, 8, 0);
        processValue += 1;
        ui->progressBar->setValue(processValue);
    }

    myCtImg = ctColorImg;
    myCtQImage = QImage(myCtImg.data, myCtImg.cols, myCtImg.rows,
                        myCtImg.step, QImage::Format_RGB888);
}

// 定时器回调：timeEdit 显示当前时间
void MainWindow::onTimeOut()
{
    ui->timeEdit->setTime(QTime::currentTime());
}


// 「开始诊断」按钮：读图 → 进度条 → 处理 → 保存
void MainWindow::on_startPushButton_clicked()
{
    ctImgRead();

    QElapsedTimer time;
    time.start();
    ui->progressBar->setMaximum(0);
    while (time.elapsed() < 5000) QCoreApplication::processEvents();

    ui->progressBar->setMaximum(100);
    ui->progressBar->setMinimum(0);

    ctImgProc();
    ui->progressBar->setValue(0);
    ctImgSave();
}



// ============================================================
//  业务数据部分（MySQL：地区树 + 病人病历增删改查）
// ============================================================


// 从 MySQL 加载地区树到 treeWidget
void MainWindow::loadRegionTree()
{
    regions.clear();
    ui->treeWidget->clear();
    ui->treeWidget->setHeaderLabel(u8"地区 / 科室");

    Db::Result rows = Db::query(
        QStringLiteral("SELECT id, parent_id, name FROM region ORDER BY id"));
    for (const Db::Row &r : rows) {
        Region reg;
        reg.id = r[0].toInt();
        reg.parentId = r[1].isEmpty() ? 0 : r[1].toInt();
        reg.name = QString::fromUtf8(r[2]);
        regions.append(reg);
    }

    QHash<int, QTreeWidgetItem *> map;
    for (const Region &reg : regions) {
        auto *item = new QTreeWidgetItem();
        item->setText(0, reg.name);
        item->setData(0, Qt::UserRole, reg.id);
        map.insert(reg.id, item);

        if (reg.parentId == 0 || !map.contains(reg.parentId))
            ui->treeWidget->addTopLevelItem(item);
        else
            map.value(reg.parentId)->addChild(item);
    }
    ui->treeWidget->expandAll();
}

// 返回某地区节点及其全部后代 id（DFS）
QVector<int> MainWindow::regionAndDescendants(int regionId) const
{
    QVector<int> ids{regionId};
    QVector<int> stack{regionId};
    while (!stack.isEmpty()) {
        int cur = stack.takeLast();
        for (const Region &r : regions) {
            if (r.parentId == cur) {
                ids.append(r.id);
                stack.append(r.id);
            }
        }
    }
    return ids;
}


// 加载某地区（含子科室）的病人到 basicTableView
void MainWindow::loadPatients(int regionId)
{
    patientModel->removeRows(0, patientModel->rowCount());

    QString sql = QStringLiteral(
        "SELECT p.id, p.name, p.sex, p.ethnic, "
        "DATE_FORMAT(p.birth,'%Y-%m-%d'), r.name "
        "FROM patient p LEFT JOIN region r ON p.region_id = r.id ");

    if (regionId > 0) {
        QVector<int> ids = regionAndDescendants(regionId);
        QStringList idList;
        for (int id : ids)
            idList << QString::number(id);
        sql += QStringLiteral("WHERE p.region_id IN (") + idList.join(',') + ") ";
    }
    sql += QStringLiteral("ORDER BY p.id");

    Db::Result rows = Db::query(sql);
    for (const Db::Row &r : rows) {
        QList<QStandardItem *> items;
        for (int c = 0; c < 5; ++c)
            items.append(new QStandardItem(QString::fromUtf8(r[c + 1])));
        items[0]->setData(r[0].toInt(), Qt::UserRole);
        patientModel->appendRow(items);
    }

    if (patientModel->rowCount() > 0)
        ui->basicTableView->selectRow(0);
}


// 返回当前树节点的地区 id（无选中返回 0）
int MainWindow::selectedRegionId() const
{
    QTreeWidgetItem *it = ui->treeWidget->currentItem();
    if (!it)
        return 0;
    return it->data(0, Qt::UserRole).toInt();
}


// 返回当前表格行的病人 id（无选中返回 0）
int MainWindow::currentPatientId() const
{
    QModelIndex idx = ui->basicTableView->currentIndex();
    if (!idx.isValid())
        return 0;
    return patientModel->index(idx.row(), 0).data(Qt::UserRole).toInt();
}

// 按病人 id 回填表单 + 病历 + 照片
void MainWindow::fillForm(int patientId)
{
    if (patientId <= 0) {
        ui->nameLineEdit->clear();
        ui->maleRadioButton->setChecked(true);
        ui->femaleRadioButton->setChecked(false);
        ui->birthDateEdit->setDate(QDate::currentDate());
        ui->ethniComboBox->setCurrentText(u8"汉");
        ui->ssnLineEdit->clear();
        ui->caseTextEdit->clear();
        ui->photoLabel->setText(u8"暂无照片");
        mPhotoBytes.clear();
        return;
    }

    Db::Result p = Db::query(QStringLiteral(
                                 "SELECT ssn,name,sex,ethnic,birth FROM patient WHERE id=")
                             + QString::number(patientId));
    if (p.isEmpty())
        return;
    const Db::Row &r = p[0];

    ui->ssnLineEdit->setText(QString::fromUtf8(r[0]));
    ui->nameLineEdit->setText(QString::fromUtf8(r[1]));
    bool male = (QString::fromUtf8(r[2]) == QString::fromUtf8(u8"男"));
    ui->maleRadioButton->setChecked(male);
    ui->femaleRadioButton->setChecked(!male);
    ui->ethniComboBox->setCurrentText(QString::fromUtf8(r[3]));
    QDate birth = QDate::fromString(QString::fromUtf8(r[4]), Qt::ISODate);
    ui->birthDateEdit->setDate(birth.isValid() ? birth : QDate::currentDate());

    Db::Result m = Db::query(QStringLiteral(
                                 "SELECT content, photo FROM medical_record WHERE patient_id=")
                             + QString::number(patientId));
    if (!m.isEmpty()) {
        ui->caseTextEdit->setPlainText(QString::fromUtf8(m[0][0]));
        mPhotoBytes = m[0][1];
        QPixmap photo;
        if (!mPhotoBytes.isEmpty() && photo.loadFromData(mPhotoBytes))
            ui->photoLabel->setPixmap(photo);
        else
            ui->photoLabel->setText(u8"暂无照片");
    } else {
        ui->caseTextEdit->clear();
        mPhotoBytes.clear();
        ui->photoLabel->setText(u8"暂无照片");
    }
}

// 在表格里定位并选中指定病人行
void MainWindow::selectPatient(int patientId)
{
    for (int row = 0; row < patientModel->rowCount(); ++row) {
        int id = patientModel->index(row, 0).data(Qt::UserRole).toInt();
        if (id == patientId) {
            ui->basicTableView->selectRow(row);
            return;
        }
    }
}

// 点击地区树节点：按该地区（含子科室）重新加载病人列表
void MainWindow::onRegionClicked(QTreeWidgetItem *item, int)
{
    if (!item)
        return;
    loadPatients(item->data(0, Qt::UserRole).toInt());
}


// 表格当前行变化：按该行病人 id 回填右侧表单
void MainWindow::onPatientRowChanged(const QModelIndex &current, const QModelIndex &)
{
    if (!current.isValid())
        return;
    int id = patientModel->index(current.row(), 0).data(Qt::UserRole).toInt();
    fillForm(id);
}


// 在当前选中地区下新增一个病人
void MainWindow::addPatient()
{
    int regionId = selectedRegionId();
    if (regionId <= 0) {
        QMessageBox::information(this, u8"提示",
                                 u8"请先在左侧选择新病人所属的地区/科室。");
        return;
    }

    QString sql = QStringLiteral(
                      "INSERT INTO patient(ssn,name,sex,ethnic,birth,region_id) "
                      "VALUES('', '新病人', '男', '汉', CURDATE(), %1)").arg(regionId);
    if (!Db::exec(sql)) {
        QMessageBox::warning(this, u8"新增失败", Db::lastError());
        return;
    }

    unsigned long long newId = mysql_insert_id(Db::handle());
    loadPatients(regionId);
    selectPatient(static_cast<int>(newId));
}


// 把表单内容 UPDATE 到 patient 表
void MainWindow::savePatient()
{
    int id = currentPatientId();
    if (id <= 0) {
        QMessageBox::information(this, u8"提示",
                                 u8"请先在表格中选择要保存的病人。");
        return;
    }

    QString name = ui->nameLineEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, u8"提示", u8"姓名不能为空。");
        return;
    }

    QString sex = ui->maleRadioButton->isChecked()
                      ? QString::fromUtf8(u8"男") : QString::fromUtf8(u8"女");
    QString ethnic = ui->ethniComboBox->currentText();
    QString ssn = ui->ssnLineEdit->text().trimmed();
    QDate birth = ui->birthDateEdit->date();

    QString sql = QStringLiteral(
                      "UPDATE patient SET ssn='%1', name='%2', sex='%3', ethnic='%4', birth='%5' "
                      "WHERE id=%6")
                      .arg(Db::escape(ssn), Db::escape(name), Db::escape(sex),
                           Db::escape(ethnic), Db::escape(birth.toString(Qt::ISODate)))
                      .arg(id);

    if (!Db::exec(sql)) {
        QMessageBox::warning(this, u8"保存失败", Db::lastError());
        return;
    }

    int regionId = selectedRegionId();
    loadPatients(regionId);
    selectPatient(id);
}


// 确认后删除当前病人（病历靠外键 ON DELETE CASCADE 级联删除）
void MainWindow::deletePatient()
{
    int id = currentPatientId();
    if (id <= 0) {
        QMessageBox::information(this, u8"提示",
                                 u8"请先在表格中选择要删除的病人。");
        return;
    }

    QString name = ui->nameLineEdit->text();
    if (QMessageBox::question(
            this, u8"确认删除",
            QString(u8"确定删除病人「%1」及其病历吗？").arg(name),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    if (!Db::exec(QStringLiteral("DELETE FROM patient WHERE id=")
                  + QString::number(id))) {
        QMessageBox::warning(this, u8"删除失败", Db::lastError());
        return;
    }

    loadPatients(selectedRegionId());
}


// 保存病历文本 + 照片到 medical_record（无则 INSERT、有则 UPDATE）
void MainWindow::saveRecord()
{
    int id = currentPatientId();
    if (id <= 0) {
        QMessageBox::information(this, u8"提示",
                                 u8"请先在表格中选择病人。");
        return;
    }

    QString content = ui->caseTextEdit->toPlainText();

    // BLOB 用 0xHEX 字面量写入；没有照片时置 NULL
    QString photoSql = mPhotoBytes.isEmpty()
                           ? QStringLiteral("NULL")
                           : QStringLiteral("0x") + QString::fromLatin1(mPhotoBytes.toHex());

    Db::Result exists = Db::query(QStringLiteral(
                                      "SELECT id FROM medical_record WHERE patient_id=") + QString::number(id));
    bool ok;
    if (exists.isEmpty())
        ok = Db::exec(QStringLiteral(
                          "INSERT INTO medical_record(patient_id, content, photo) VALUES(%1,'%2',%3)")
                          .arg(QString::number(id), Db::escape(content), photoSql));
    else
        ok = Db::exec(QStringLiteral(
                          "UPDATE medical_record SET content='%1', photo=%2 WHERE patient_id=%3")
                          .arg(Db::escape(content), photoSql, QString::number(id)));

    if (!ok)
        QMessageBox::warning(this, u8"保存失败", Db::lastError());
    else
        QMessageBox::information(this, u8"完成", u8"病历与照片已保存。");
}


// 选照片 → 统一转 JPG 字节 → 预览缩略图
void MainWindow::uploadPhoto()
{
    int id = currentPatientId();
    if (id <= 0) {
        QMessageBox::information(this, u8"提示",
                                 u8"请先在表格中选择病人。");
        return;
    }

    QString file = QFileDialog::getOpenFileName(this, u8"选择病人照片", ".",
                                                "Image File(*.png *.jpg *.jpeg *.bmp)");
    if (file.isEmpty())
        return;

    QImage img(file);
    if (img.isNull()) {
        QMessageBox::warning(this, u8"载入失败", u8"无法读取该图片，请确认文件格式正确。");
        return;
    }

    // 统一转成 JPG 存库，便于预览与后续扩展
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!img.save(&buffer, "JPG")) {
        QMessageBox::warning(this, u8"载入失败", u8"图片编码失败。");
        return;
    }
    buffer.close();

    mPhotoBytes = bytes;
    QPixmap pm;
    pm.loadFromData(bytes, "JPG");
    ui->photoLabel->setPixmap(pm);

    QMessageBox::information(this, u8"完成",
                             u8"照片已选择，点击「保存病历」后写入数据库。");
}


// 用代码创建底部「新增/保存/删除/保存病历/上传照片」按钮并连接槽
void MainWindow::buildCrudButtons()
{
    auto *btnAdd = new QPushButton(u8"新增病人", this);
    auto *btnSave = new QPushButton(u8"保存信息", this);
    auto *btnDel = new QPushButton(u8"删除病人", this);
    auto *btnRec = new QPushButton(u8"保存病历", this);

    btnAdd->setGeometry(310, 672, 95, 31);
    btnSave->setGeometry(415, 672, 95, 31);
    btnDel->setGeometry(520, 672, 95, 31);
    btnRec->setGeometry(625, 672, 95, 31);

    connect(btnAdd, &QPushButton::clicked, this, &MainWindow::addPatient);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::savePatient);
    connect(btnDel, &QPushButton::clicked, this, &MainWindow::deletePatient);
    connect(btnRec, &QPushButton::clicked, this, &MainWindow::saveRecord);

    // 照片上传按钮，放在“信息”页照片缩略图下方
    auto *btnPhoto = new QPushButton(u8"上传照片", ui->frame);
    btnPhoto->setGeometry(175, 80, 66, 26);
    connect(btnPhoto, &QPushButton::clicked, this, &MainWindow::uploadPhoto);
}
