#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "visa.h"
#include <map>
#include <vector>
#include <string>

using namespace std;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btn_start_clicked();

private:
    Ui::MainWindow *ui;
    quint64 step_uint64 = 0;
    QString step_qstr = "";
    quint64 freq_start = 3600000000;
    quint64 freq_stop = 26500000000;
    quint64 freq_current = 0;
    QString atten_qstr = "";
    QString YTFNumber = "";
    ViSession rmSession;
    ViSession networkAnalyzer;
    ViSession signalAnalyzer;
    ViSession digitMultimeter;
    const unsigned long int DEFAULT_TMO = 5000;
    const unsigned int MAX_SCPI_LEN = 255;
    map<quint64, vector<QString>> result;


private:
    bool measureSPara(QString sPara);//网分测量参数
    bool signalAnalyzerSet(QString f, QString span);//设置频谱的中心频率和SPAN
    bool networkAnalyzerSet(QString f, QString span);//设置网分的中心频率和SPAN
    bool networkAnalyzerAtten(QString atten);//设置网分的衰减时-1DB还是-3DB
    bool networkAnalyzerSearch();//读取attenDB点
    bool networkAnalyzerFlatness(QString newSpan);//平坦度测试
    bool setAttribute(ViSession ptr);//返回true表示设置属性成功，返回false表示设置属性失败
    bool digitMultimeterReadCurrentValue();//测量电流值
    bool writeToCSV(QString fileName);//将测试结果写入CSV中





};

#endif // MAINWINDOW_H
