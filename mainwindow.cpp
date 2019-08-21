#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QMessageBox>


void mySleep(int msec)
{
    QDateTime reachTime = QDateTime::currentDateTime().addMSecs(msec);
    while(QDateTime::currentDateTime() < reachTime){
        QCoreApplication::processEvents(QEventLoop::AllEvents, 500);
    }
}


bool writeInformationToFileWithCurrentTime(QString strInformation, QString flag)
{
    //flag == "[SCPI]"表示写入仪器的指令
    //flag == "[RESPONSE]"表示仪器反馈给机台的信息
    QDateTime timeStart = QDateTime::currentDateTime();//获取系统现在时间
    QString str = timeStart.toString("yyyy-MM-dd hh:mm:ss.zzz");//显示设置格式

    QFile file("YTF.log");
    if(!file.open(QIODevice::Append | QIODevice::Text)){
        return false;
    }
    QTextStream out(&file);

    out << str << " " << "[GPIB]" << " " << "[" + flag + "]" << " " << strInformation<<"\n";

    file.close();
    return true;
}



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btn_start_clicked()
{
    result.clear();

    YTFNumber = this->ui->le_YTFNumber->text();
    if(YTFNumber == "")
    {
        QMessageBox::information(this, "Error", "Please input YTFNumber");
        return;
    }

    //1. openDefaultRM
    ViStatus nRetStatus = viOpenDefaultRM(&rmSession);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("viOpenDefaultRM FAIL", "SCPI");
        this->ui->lb_result->setText("Error");
        this->ui->btn_start->setEnabled(true);
        return;
    }

    writeInformationToFileWithCurrentTime("viOpenDefaultRM SUCCESS", "SCPI");


    //2.open
    nRetStatus = viOpen(rmSession, this->ui->le_networkAnalyzer->text().toStdString().c_str(), VI_NULL, VI_NULL, &networkAnalyzer);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("viOpen networkAnalyzer fail", "SCPI");

        this->ui->lb_result->setText("Error");
        this->ui->btn_start->setEnabled(true);
        return;
    }

    writeInformationToFileWithCurrentTime("viOpen networkAnalyzer success", "SCPI");





    nRetStatus = viOpen(rmSession, this->ui->le_signalAnalyzer->text().toStdString().c_str(), VI_NULL, VI_NULL, &signalAnalyzer);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("viOpen signalAnalyzer fail", "SCPI");

        this->ui->lb_result->setText("Error");
        this->ui->btn_start->setEnabled(true);
        return;
    }

    writeInformationToFileWithCurrentTime("viOpen signalAnalyzer success", "SCPI");


    nRetStatus = viOpen(rmSession, this->ui->le_digitMultimeter->text().toStdString().c_str(), VI_NULL, VI_NULL, &digitMultimeter);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("viOpen digitMultimeter fail", "SCPI");

        this->ui->lb_result->setText("Error");
        this->ui->btn_start->setEnabled(true);
        return;
    }

    writeInformationToFileWithCurrentTime("viOpen digitMultimeter success", "SCPI");





    //获取UI界面的值
    step_qstr = this->ui->cb_step->currentText();
    QString atten = this->ui->cb_atten->currentText();
    if(atten == "-1DB"){
        atten_qstr = "-1";
    }else{
        atten_qstr = "-3";
    }
    if(step_qstr == "20M"){
        step_uint64 = 20000000;
    }else{
        //40M
        step_uint64 = 40000000;
    }

    this->ui->btn_start->setEnabled(false);


    //设置S21量测
    measureSPara("S21");



    mySleep(500);

    //测量范围确定
    //起始频率为3600M,结束频率为26500M
    this->ui->lb_result->setText("Testing...");
    quint64 i = 0;

    while(1){
        //test
        freq_current = freq_start + i * step_uint64;

//        if(i == 1){

//            //test
//            break;
//        }



        if(freq_current > freq_stop){
            //测试完成
            break;
        }

        //设置频谱的中心频率和SPAN
        signalAnalyzerSet(QString::number(freq_current), "5000000");
        networkAnalyzerSet(QString::number(freq_current), "500000000");
        mySleep(1000);



        //读取电流
        digitMultimeterReadCurrentValue();
        mySleep(1000);

        //设置-1或者-3DB
        networkAnalyzerAtten(atten_qstr);
        mySleep(1000);
        //Search
        networkAnalyzerSearch();
        //平坦度测试
        mySleep(1000);
        networkAnalyzerFlatness("40000000");
        mySleep(500);


        //设置为连续测试
        nRetStatus = viPrintf(networkAnalyzer, "SENS:SWE:MODE CONT\n", 0);
        if(nRetStatus != VI_SUCCESS){
            writeInformationToFileWithCurrentTime("SENS:SWE:MODE Single CONT", "SCPI");
            return;
        }
        writeInformationToFileWithCurrentTime("SENS:SWE:MODE CONT", "SCPI");



        ++i;
    }

    writeToCSV(this->ui->le_YTFNumber->text());



    //关闭com
    viClose(signalAnalyzer);
    viClose(networkAnalyzer);
    viClose(digitMultimeter);
    viClose(rmSession);



    this->ui->lb_result->setText("Finished");
    this->ui->btn_start->setEnabled(true);



}


bool MainWindow::setAttribute(ViSession ptr)//返回true表示设置属性成功，返回false表示设置属性失败
{
    ViStatus retStatus1 = viSetAttribute(ptr, VI_ATTR_TMO_VALUE, DEFAULT_TMO);
    ViStatus retStatus2 = viSetAttribute(ptr, VI_ATTR_SUPPRESS_END_EN, VI_FALSE);
    ViStatus retStatus3 = viSetAttribute(ptr, VI_ATTR_SEND_END_EN, VI_TRUE);
    if((retStatus1 == VI_SUCCESS) && (retStatus2 == VI_SUCCESS) && (retStatus3 == VI_SUCCESS)){
        return true;
    }
    return false;
}

bool MainWindow::digitMultimeterReadCurrentValue()
{
    ViStatus nRetStatus = viPrintf(digitMultimeter, "MEASURE:CURRENT:DC?\n", 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("MEASURE:CURRENT:DC? FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("MEASURE:CURRENT:DC?", "SCPI");

    mySleep(1000);

    ViByte rdBuff[255] = {};
    nRetStatus = viScanf(digitMultimeter, "%s", rdBuff);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("MEASURE:CURRENT:DC? FAIL", "RESPONSE");
        return false;
    }
    writeInformationToFileWithCurrentTime("MEASURE:CURRENT:DC?", "RESPONSE");

    //对读出的数据进行处理
    string str = (char *)rdBuff;
    QString qstr = QString::fromStdString(str);

    result[freq_current].push_back(qstr);
//    qDebug()<<qstr;
    return true;
}

bool MainWindow::writeToCSV(QString fileName)
{
    //将测试结果写入CSV中
    QFile file(fileName+".csv");
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)){
        return false;
    }
    QTextStream out(&file);

    out<<step_qstr<<","<<"Current(mA)"<<","<<"YTF(Central Frequency GHz)"<<","<<("BW("+atten_qstr+")dB(MHz)")<<","<<"LOSS(dB)"<<","<<"Flatness(MAX-MIN dB)"<<","<<"Offset(MHz)"<<","<<"|BW - Offset|(MHz)"<<"\n";

    for(map<quint64, vector<QString>>::iterator it = result.begin(); it != result.end(); ++it){

        out<<QString::number(QString::number(it->first).toDouble() / 1e9, 'f', 2)<<",";//frequency

        out<<QString::number((it->second.at(0)).toDouble() * 1e3, 'f', 2)<<",";//Current

        out<<QString::number((it->second.at(1)).toDouble() / 1e9, 'f', 4)<<",";//YTF(Central)

        out<<QString::number((it->second.at(2)).toDouble() / 1e6, 'f', 2)<<",";//BW

        out<<QString::number((it->second.at(3)).toDouble() , 'f', 2)<<",";//LOSS

        out<<QString::number((it->second.at(4)).toDouble() , 'f', 2)<<",";//Flatness






//        for(vector<QString>::iterator it_data = it->second.begin(); it_data != it->second.end(); ++it_data){
//            out<<"'" + *it_data<<",";
//        }
        //最后两项计算的结果
        out<<"'" + QString::number(it->first - it->second[1].toDouble() / 1e6, 'f', 2)<<",";

        out<<"'" + QString::number(fabs(it->second[2].toDouble() - fabs(it->first - it->second[1].toDouble())) / 1e6, 'f', 2);

        out<<"\n";
    }



    file.close();
    return true;


}

bool MainWindow::measureSPara(QString sPara)
{
    ViStatus nRetStatus = viPrintf(networkAnalyzer, ("CALCulate1:MEASure1:PARameter '" + sPara + "'\n").toStdString().c_str(), 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("CALCulate1:MEASure1:PARameter "+sPara+ "FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("CALCulate1:MEASure1:PARameter "+sPara, "SCPI");
    nRetStatus = viPrintf(networkAnalyzer, "*WAI\n", 0);
    return true;
}

bool MainWindow::signalAnalyzerSet(QString f, QString span)
{
    //设置中心频率
    ViStatus nRetStatus = viPrintf(signalAnalyzer, ("FREQ:CENT " + f + "\n").toStdString().c_str(), 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("FREQ:CENT " + f + " FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("FREQ:CENT " + f, "SCPI");
    //设置SPAN
    nRetStatus = viPrintf(signalAnalyzer, ("FREQ:SPAN " + span + "\n").toStdString().c_str(), 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("FREQ:SPAN " + span + " FAIL", "SCPI");
        return false;
    }

    writeInformationToFileWithCurrentTime("FREQ:SPAN " + span, "SCPI");

    return true;
}

bool MainWindow::networkAnalyzerSet(QString f, QString span)
{
    //设置中心频率
    ViStatus nRetStatus = viPrintf(networkAnalyzer, ("SENSe:FREQuency:CENTer " + f + "\n").toStdString().c_str(), 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("SENSe:FREQuency:CENTer " + f + " FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("SENSe:FREQuency:CENTer " + f, "SCPI");
    //设置SPAN
    nRetStatus = viPrintf(networkAnalyzer, ("SENSe:FREQ:SPAN " + span + "\n").toStdString().c_str(), 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("SENSe:FREQ:SPAN " + span + " FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("SENSe:FREQ:SPAN " + span, "SCPI");
    return true;
}

bool MainWindow::networkAnalyzerAtten(QString atten)
{
    //设置-1还是-3
    ViStatus nRetStatus = viPrintf(networkAnalyzer, ("CALC:MEAS:MARK:BWID:THR " + atten + "\n").toStdString().c_str(), 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("CALC:MEAS:MARK:BWID:THR " + atten + " FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("CALC:MEAS:MARK:BWID:THR " + atten, "SCPI");
    return true;
}

bool MainWindow::networkAnalyzerSearch()
{
    ViStatus nRetStatus = viPrintf(networkAnalyzer, "CALC:MEASure:MARKer1:BWIDth ON\n", 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("CALC:MEASure:MARKer1:BWIDth ON FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("CALC:MEASure:MARKer1:BWIDth ON", "SCPI");

    mySleep(1000);

    nRetStatus = viPrintf(networkAnalyzer, "CALC:MEASure:MARKer1:BWIDth:DATA?\n", 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("CALC:MEASure:MARKer1:BWIDth:DATA? FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("CALC:MEASure:MARKer1:BWIDth:DATA?", "SCPI");



    ViByte rdBuff[255] = {};
    nRetStatus = viScanf(networkAnalyzer, "%s", rdBuff);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("CALC:MEASure:MARKer1:BWIDth:DATA? FAIL", "RESPONSE");
        return false;
    }
    writeInformationToFileWithCurrentTime("CALC:MEASure:MARKer1:BWIDth:DATA?", "RESPONSE");



    //对读出的数据进行处理
    string str = (char *)rdBuff;
    QString qstr = QString::fromStdString(str);
    QStringList qstrList = qstr.split(",");
    //qDebug()<<qstrList;
    result[freq_current].push_back(qstrList.at(1));
    result[freq_current].push_back(qstrList.at(0));
    result[freq_current].push_back(qstrList.at(3));
    return true;
}

bool MainWindow::networkAnalyzerFlatness(QString newSpan)
{
    //将Marker4移动到中间
    ViStatus nRetStatus = viPrintf(networkAnalyzer, "CALC:MEASure:MARKer4:SET CENT\n", 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("CALC:MEASure:MARKer4:SET CENT FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("CALC:MEASure:MARKer4:SET CENT", "SCPI");

    //将span改为40M
    nRetStatus = viPrintf(networkAnalyzer, ("SENSe:FREQuency:SPAN " + newSpan + "\n").toStdString().c_str(), 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("SENSe:FREQuency:SPAN " + newSpan + "FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("SENSe:FREQuency:SPAN " + newSpan, "SCPI");


    //设置为单次测试
    nRetStatus = viPrintf(networkAnalyzer, "SENS:SWE:MODE Single\n", 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("SENS:SWE:MODE Single FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("SENS:SWE:MODE Single", "SCPI");


    mySleep(1000);


  //选出最大值
    nRetStatus = viPrintf(networkAnalyzer, "CALC:MEAS:MARK4:FUNC:EXEC MAX\n", 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:FUNC:EXEC MAX FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:FUNC:EXEC MAX", "SCPI");

    mySleep(1500);


//  //选择Delta
//    nRetStatus = viPrintf(networkAnalyzer, "CALC:MEAS:MARK4:DELT ON\n", 0);
//    if(nRetStatus != VI_SUCCESS){
//        writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:DELT ON FAIL", "SCPI");
//        return false;
//    }
//    writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:DELT ON", "SCPI");

    //读取Marker4的值
       nRetStatus = viPrintf(networkAnalyzer, "CALC:MEAS:MARK4:Y?\n", 0);
       if(nRetStatus != VI_SUCCESS){
           writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:Y? FAIL", "SCPI");
           return false;
       }
       writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:Y?", "SCPI");

       ViByte rdBuff[255] = {};
       nRetStatus = viScanf(networkAnalyzer, "%s", rdBuff);
       if(nRetStatus != VI_SUCCESS){
           writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:Y? FAIL", "RESPONSE");
           return false;
       }
       writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:Y?", "RESPONSE");

       //对读出的数据进行处理
       string str = (char *)rdBuff;
       QString qstr = QString::fromStdString(str);
       //qDebug()<<"MAX"<<qstr;
       double Max = qstr.split(',').at(0).toDouble();

  //选出最小值
    nRetStatus = viPrintf(networkAnalyzer, "CALC:MEAS:MARK4:FUNC:EXEC MIN\n", 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:FUNC:EXEC MIN FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:FUNC:EXEC MIN", "SCPI");

    mySleep(1500);


 //读取Marker4的值
    nRetStatus = viPrintf(networkAnalyzer, "CALC:MEAS:MARK4:Y?\n", 0);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:Y? FAIL", "SCPI");
        return false;
    }
    writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:Y?", "SCPI");


    ViByte rdBuff1[255] = {};
    nRetStatus = viScanf(networkAnalyzer, "%s", rdBuff1);
    if(nRetStatus != VI_SUCCESS){
        writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:Y? FAIL", "RESPONSE");
        return false;
    }
    writeInformationToFileWithCurrentTime("CALC:MEAS:MARK4:Y?", "RESPONSE");


    //对读出的数据进行处理
    string str1 = (char *)rdBuff1;
    QString qstr1 = QString::fromStdString(str1);
    //qDebug()<<"MIN"<<qstr1;
    double Min = qstr1.split(',').at(0).toDouble();

    result[freq_current].push_back(QString::number(Max - Min));
//    qDebug()<<"MAX"<<Max;
//    qDebug()<<"Min"<<Min;
//    qDebug()<<"Max-Min"<<Max-Min;

    return true;
}


