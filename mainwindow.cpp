#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <qlistwidget.h>
#include <qdebug.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_btn_add_clicked()
{
    if (ui->txt_ip->text() != "")
    {
        if(server.AddClient(ui->txt_ip->text().toStdString(), ui->txt_port->value()) ==SUCCESS)
        {
            ui->listWidget->addItem(ui->txt_ip->text() + ":" + QString::number(ui->txt_port->value()));
        }
    }
}

void MainWindow::on_listWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    ui->btn_remove->setEnabled(true);
}



void MainWindow::on_btn_remove_clicked()
{

    QString client = ui->listWidget->selectedItems().first()->text();
    if (server.RemoveClient(client.split(':').at(0).toStdString(), client.split(':').at(1).toInt()) == SUCCESS)
    {
        qDebug() << "client " << client << "removed.";
    }
    else
    {
        qDebug() << "client " << client << "does not exist.";
    }
    ui->listWidget->takeItem(ui->listWidget->currentRow());
    if(ui->listWidget->count() == 0)
    {
        ui->btn_remove->setEnabled(false);
    }
}
