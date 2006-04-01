/** @file
 AboutWindow - class representing about window. Display authors, application name and version
*/
#include "aboutwindow.h"
#include "settings.h"
#include <iostream>
#include <qlayout.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include "version.h"

using namespace std;

/** Version of program*/
QString app=APP_NAME " " VERSION;

/** constructor of AboutWindow, creates window and fills it with elements, parameters are ignored */
AboutWindow::AboutWindow(QWidget *parent/*=0*/,const char *name/*=0*/):QWidget(parent,name,WDestructiveClose || WType_TopLevel) {
 QString info=QString("<big>")+tr("PDF editor for unix systems")+"</big>";
 QString authors=QString("<b>")+tr("Project leader")+":</b><br>&nbsp; Martin Beran<br><b>"
                               +tr("Authors")+":</b><br>&nbsp; Michal Hocko<br>&nbsp; Miro Jahoda<br>&nbsp; Josef Misutka<br>&nbsp; Martin Petricek<br>";
 setCaption(tr("About program"));
 QBoxLayout *l=new QVBoxLayout(this);
// QVBox *qb=new QVBox(this,"about_vbox");
 QLabel *lb=new QLabel(QString("<table><tr><td valign=\"top\"><h1>")+app+"</h1><br>"+tr("Compiled")+": "+COMPILE_TIME+"<br><br>"+
  info+"</td><td valign=\"bottom\">"+authors+"</td></tr><tr><td colspan=\"\2\">"+tr("This program is distributed under terms of GNU GPL")+"</td></tr></table>", this);
 lb->setTextFormat(Qt::RichText);
 QPushButton *ok=new QPushButton(tr("Ok"), this);
 QObject::connect(ok, SIGNAL(clicked()), this, SLOT(close()));
 l->addWidget(lb);
 l->addWidget(ok);
 ok->show();
 lb->show();
}

/** default destructor */
AboutWindow::~AboutWindow() {
}
