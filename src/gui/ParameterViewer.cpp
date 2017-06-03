#include "ParameterViewer.h"
#include "StringRegistry.h"
#include "MainWindow.h"
#include "Drawing.h"
#include "Icons.h"
#include "Json.h"
#include <sstream>
#include <iostream>


ParameterViewer::ParameterViewer(QWidget *parent, MainWindow *mainwindow) : QAbstractScrollArea(parent) {
	m_mainwindow = mainwindow;
	m_hover_region = HOVER_REGION_NONE;
	m_button_pressed = false;
	m_current_index = INDEX_NONE;
	m_current_subindex = INDEX_NONE;

	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	//setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn); // buggy with GTK style, maximumViewportSize() doesn't take the border into account :(
	verticalScrollBar()->setSingleStep(10);
	setFocusPolicy(Qt::NoFocus);

	viewport()->setBackgroundRole(QPalette::Window);
	viewport()->setAutoFillBackground(true);
	viewport()->setMouseTracking(true);

	UpdateRange();

}

ParameterViewer::~ParameterViewer() {}


static QSize GetWidgetSize(QWidget* widget)
{
	QSize size = widget->sizeHint();
	if(!size.isValid())
		size = QSize(0, 0);
	QSize minimumsize = widget->minimumSize();
	if(!minimumsize.isValid())
		minimumsize = widget->minimumSizeHint();
	if(minimumsize.isValid())
		size.expandedTo(minimumsize);
	QSize maximumsize = widget->maximumSize();
	if(maximumsize.isValid())
		size.boundedTo(maximumsize);
	return size;
}

index_t ParameterViewer::GetWidgetCount()
{
	return m_parameters.GetSize();
}

QWidget* ParameterViewer::GetWidget(index_t index)
{
	assert(index < m_parameters.GetSize());
	return m_parameters[index].m_widget;
}

//void ParameterViewer::AddWidget(index_t index, QWidget* widget)
//{
//	assert(index <= m_parameters.GetSize());
//	assert(widget->parent() == viewport());

//	widget->setAutoFillBackground(true);
//	widget->setBackgroundRole(QPalette::Window);
//	if(widget->focusPolicy() == Qt::NoFocus)
//		widget->setFocusPolicy(Qt::ClickFocus);
//	widget->show();

//	m_widgets.insert(m_widgets.begin() + index, widget);
//	UpdateFocusChain();
//	UpdateRange();
//	UpdateLayout();

//}

//void ParameterViewer::RemoveWidget(index_t index)
//{
//	assert(index < m_parameters.GetSize());

//	delete m_widgets[index];
//	m_widgets.erase(m_widgets.begin() + index);
//	UpdateFocusChain();
//	UpdateRange();
//	UpdateLayout();

//}

//void ParameterViewer::MoveWidget(index_t from, index_t to)
//{
//	assert(from < m_parameters.GetSize());
//	assert(to < m_parameters.GetSize());

//	QWidget *widget = m_widgets[from];
//	m_widgets.erase(m_widgets.begin() + from);
//	m_widgets.insert(m_widgets.begin() + to, widget);
//	UpdateFocusChain();
//	UpdateLayout();

//}

void ParameterViewer::MakeVisible(QWidget* widget)
{
	assert(viewport()->isAncestorOf(widget));
	QRect widget_rect(widget->mapTo(viewport(), QPoint(0, verticalScrollBar()->value())), widget->size());
	QRect visible_rect(QPoint(0, verticalScrollBar()->value()), viewport()->size());
	if(widget_rect.height() > visible_rect.height()) {
		if(widget_rect.top() > visible_rect.top()) {
			verticalScrollBar()->setValue(widget_rect.top());
		} else if(widget_rect.bottom() < visible_rect.bottom()) {
			verticalScrollBar()->setValue(visible_rect.top() + widget_rect.bottom() - visible_rect.bottom());
		}
	} else {
		if(widget_rect.top() < visible_rect.top()) {
			verticalScrollBar()->setValue(widget_rect.top());
		} else if(widget_rect.bottom() > visible_rect.bottom()) {
			verticalScrollBar()->setValue(visible_rect.top() + widget_rect.bottom() - visible_rect.bottom());
		}
	}
}

void ParameterViewer::UpdateParameters()
{
	const std::vector<Cow<::Shape>>& shapes = m_mainwindow->GetDocumentViewer()->GetActiveDocument()->GetDrawing()->GetShapes();
	HashTable<ParameterEntry, ParameterHasher> new_parameters;

	// make new m_parameter
	// copy existing widgets + expanded info
	for(index_t i = 0 ; i < shapes.size(); ++i) {
		const ::Shape &shaperef = shapes[i].Ref();
		VData::Dict params = shaperef.GetParams().Ref();

		for(index_t j = 0 ; j < params.GetSize(); ++j) {
			// does the parameter already exist in new_parameter
			//		if Yes is it mergable or not
			//			if No set bools + change widget text
			// if No does the parameter already exist in m_parameters
			//		if Yes copy widget pointer + expanded
			//		if No make widget

			std::pair<index_t, bool> added = new_parameters.TryEmplaceBack(params[j].Key(),params[j].Key(),params[j].Value(),true,true,false,nullptr);

			if(!added.second) { // parameter exists
				if(new_parameters[added.first].m_value != params[j].Value()) { // not mergable
					new_parameters[added.first].m_mergeable = false;
					static_cast<QLineEdit*>(new_parameters[added.first].m_widget)->setText("...");
				}
			}
			else {
				index_t index = m_parameters.Find(params[j].Key());
				if(index == INDEX_NONE) {
					QLineEdit *valuebox = new QLineEdit(viewport());
					std::string str;
					Json::ToString(params[j].Value(),str);
					valuebox->setText(QString::fromStdString(str));
					valuebox->setAutoFillBackground(true);
					valuebox->setBackgroundRole(QPalette::Window);
					if(valuebox->focusPolicy() == Qt::NoFocus)
						valuebox->setFocusPolicy(Qt::ClickFocus);
					valuebox->show();
					new_parameters[added.first].m_widget = valuebox;
				}
				else { // copy widget pointer
					new_parameters[added.first].m_widget = m_parameters[index].m_widget;
					new_parameters[added.first].m_expanded = m_parameters[index].m_expanded;
					m_parameters[index].m_widget = NULL;
				}
			}
		}

	}

	// remove unused widgets
	for(index_t i = 0 ; i < m_parameters.GetSize(); ++i) {
		if(m_parameters[i].m_widget != NULL) {
			m_parameters[i].m_widget->deleteLater();
			for(index_t j = 0 ; j < m_parameters[i].m_subparameters.size(); ++j) {
				m_parameters[i].m_subparameters[j].m_widget->deleteLater();
			}
		}
	}

	// copy new parameters to m_parameter
	m_parameters = std::move(new_parameters);

	UpdateFocusChain();
	UpdateRange();
	UpdateLayout();

}

bool ParameterViewer::viewportEvent(QEvent* event)
{
	switch(event->type()) {
		case QEvent::LayoutRequest: {
			UpdateRange();
			UpdateLayout();
			event->accept();
			return true;
		}
		default: return QAbstractScrollArea::viewportEvent(event);
	}
}

void ParameterViewer::resizeEvent(QResizeEvent* event)
{
	Q_UNUSED(event);
	UpdateRange();
	UpdateLayout();
}

void ParameterViewer::scrollContentsBy(int dx, int dy)
{
	if(dx == 0 && dy == 0)
		return;
	UpdateLayout();
}

bool ParameterViewer::focusNextPrevChild(bool next) {
	if (QWidget::focusNextPrevChild(next)) {
		if (QWidget *fw = focusWidget())
			ensureWidgetVisible(fw);
		return true;
	}
	return false;
}

void ParameterViewer::paintEvent(QPaintEvent *event)
{
	UNUSED(event);
	QPainter painter(viewport());

	int y = -verticalScrollBar()->value();
	y = y + LAYOUT_VSPACING;

	for(index_t i = 0; i < m_parameters.GetSize(); ++i)
	{
		QSize size = GetWidgetSize(m_parameters[i].m_widget);

		// INDEX FOLDBUTTON
		{
			QStyleOptionButton opt_foldbutton;
			opt_foldbutton.state = QStyle::State_Item | QStyle::State_Active | QStyle::State_Enabled;

			// FOLDBUTTON EXPANDED/UNEXPANDED STATE
			if(m_parameters[i].m_expanded && !m_parameters[i].m_mergeable){
				opt_foldbutton.state |= QStyle::State_Children | QStyle::State_Open;
			}
			else if(!m_parameters[i].m_expanded && !m_parameters[i].m_mergeable){
				opt_foldbutton.state |= QStyle::State_Children;
			}

			if(i < m_parameters.GetSize()-1){
				opt_foldbutton.state |= QStyle::State_Sibling;
			}

			// FOLDBUTTON MOUSE OVER
			if(m_current_index == i && m_current_subindex == INDEX_NONE && (m_hover_region == HOVER_REGION_FOLDBUTTON || m_hover_region == HOVER_REGION_LABEL)){
				opt_foldbutton.state |= QStyle::State_MouseOver;
			}
			else {
				opt_foldbutton.state &= ~QStyle::State_MouseOver;
			}

			opt_foldbutton.rect = QRect(0,y-LAYOUT_VSPACING/2,LAYOUT_FOLDBUTTONWIDTH,size.height()+LAYOUT_VSPACING);
			style()->drawPrimitive(QStyle::PE_IndicatorBranch, &opt_foldbutton, &painter, viewport());
		}

		// INDEX LABEL
		{
			painter.drawText(QRect(LAYOUT_FOLDBUTTONWIDTH+LAYOUT_HSPACING,y,LAYOUT_LABELWIDTH-LAYOUT_FOLDBUTTONWIDTH+LAYOUT_HSPACING,size.height()),Qt::AlignLeft|Qt::AlignVCenter|Qt::TextSingleLine,QString::fromStdString(StringRegistry::GetString(m_parameters[i].m_name)));
		}

		// INDEX OVERRIDE BUTTON
		{
			// ON STATE
			if(m_parameters[i].m_override){
				// PRESSED
				if(m_button_pressed && m_hover_region == HOVER_REGION_OVERRIDEBUTTON && m_current_index == i &&  m_current_subindex == INDEX_NONE){
					g_icon_parameterviewer_override_onpressed.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH,y+(size.height()-LAYOUT_OVERRIDEBUTTONWIDTH)/2,LAYOUT_OVERRIDEBUTTONWIDTH,LAYOUT_OVERRIDEBUTTONWIDTH);
				}
				// MOUSE OVER
				else if(m_hover_region == HOVER_REGION_OVERRIDEBUTTON && m_current_index == i &&  m_current_subindex == INDEX_NONE){
					g_icon_parameterviewer_override_onmouseover.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH,y+(size.height()-LAYOUT_OVERRIDEBUTTONWIDTH)/2,LAYOUT_OVERRIDEBUTTONWIDTH,LAYOUT_OVERRIDEBUTTONWIDTH);
				}
				// NORMAL
				else{
					g_icon_parameterviewer_override_onnormal.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH,y+(size.height()-LAYOUT_OVERRIDEBUTTONWIDTH)/2,LAYOUT_OVERRIDEBUTTONWIDTH,LAYOUT_OVERRIDEBUTTONWIDTH);
				}
			}
			// OFF STATE
			else{
				// PRESSED
				if(m_button_pressed && m_hover_region == HOVER_REGION_OVERRIDEBUTTON && m_current_index == i &&  m_current_subindex == INDEX_NONE){
					g_icon_parameterviewer_override_offpressed.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH,y+(size.height()-LAYOUT_OVERRIDEBUTTONWIDTH)/2,LAYOUT_OVERRIDEBUTTONWIDTH,LAYOUT_OVERRIDEBUTTONWIDTH);
				}
				// MOUSE OVER
				else if(m_hover_region == HOVER_REGION_OVERRIDEBUTTON && m_current_index == i &&  m_current_subindex == INDEX_NONE){
					g_icon_parameterviewer_override_offmouseover.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH,y+(size.height()-LAYOUT_OVERRIDEBUTTONWIDTH)/2,LAYOUT_OVERRIDEBUTTONWIDTH,LAYOUT_OVERRIDEBUTTONWIDTH);
				}
				// NORMAL
				else{
					g_icon_parameterviewer_override_offnormal.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH,y+(size.height()-LAYOUT_OVERRIDEBUTTONWIDTH)/2,LAYOUT_OVERRIDEBUTTONWIDTH,LAYOUT_OVERRIDEBUTTONWIDTH);
				}
			}
		}



		y += size.height()+LAYOUT_VSPACING;


		if(!m_parameters[i].m_mergeable && m_parameters[i].m_expanded){
			for(index_t j = 0; j < m_parameters[i].m_subparameters.size(); ++j){

				// SUBINDEX FOLDBUTTON
				if(i < m_parameters.GetSize()-1){
					QStyleOptionButton opt_foldbutton;
					opt_foldbutton.state = QStyle::State_Active | QStyle::State_Enabled | QStyle::State_Sibling;
					opt_foldbutton.rect = QRect(0,y-LAYOUT_VSPACING/2,LAYOUT_FOLDBUTTONWIDTH,size.height()+LAYOUT_VSPACING);
					style()->drawPrimitive(QStyle::PE_IndicatorBranch, &opt_foldbutton, &painter, viewport());
				}

				// SUBINDEX SELECTBUTTON
				{

					// PRESSED
					if(m_button_pressed && m_hover_region == HOVER_REGION_SELECTBUTTON && m_current_index == i &&  m_current_subindex == j){
						g_icon_parameterviewer_select_pressed.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+LAYOUT_HSPACING,y,LAYOUT_SUBPARAMBUTTONWIDTH,size.height());
					}
					// MOUSE OVER
					else if(m_hover_region == HOVER_REGION_SELECTBUTTON && m_current_index == i && m_current_subindex == j){
						g_icon_parameterviewer_select_mouseover.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+LAYOUT_HSPACING,y,LAYOUT_SUBPARAMBUTTONWIDTH,size.height());
					}
					// NORMAL
					else{
						g_icon_parameterviewer_select_normal.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+LAYOUT_HSPACING,y,LAYOUT_SUBPARAMBUTTONWIDTH,size.height());
					}
				}

				// SUBINDEX DESELECTBUTTON
				{

					// PRESSED
					if(m_button_pressed && m_hover_region == HOVER_REGION_DESELECTBUTTON && m_current_index == i &&  m_current_subindex == j){
						g_icon_parameterviewer_deselect_pressed.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_SUBPARAMBUTTONWIDTH,y,LAYOUT_SUBPARAMBUTTONWIDTH,size.height());
					}
					// MOUSE OVER
					else if(m_hover_region == HOVER_REGION_DESELECTBUTTON && m_current_index == i && m_current_subindex == j){
						g_icon_parameterviewer_deselect_mouseover.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_SUBPARAMBUTTONWIDTH,y,LAYOUT_SUBPARAMBUTTONWIDTH,size.height());
					}
					// NORMAL
					else{
						g_icon_parameterviewer_deselect_normal.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_SUBPARAMBUTTONWIDTH,y,LAYOUT_SUBPARAMBUTTONWIDTH,size.height());
					}
				}

				// SUBINDEX LABEL
				{
					painter.drawText(QRect(LAYOUT_FOLDBUTTONWIDTH+3*LAYOUT_HSPACING+2*LAYOUT_SUBPARAMBUTTONWIDTH,y,LAYOUT_LABELWIDTH-LAYOUT_FOLDBUTTONWIDTH+LAYOUT_HSPACING,size.height()),Qt::AlignLeft|Qt::AlignVCenter|Qt::TextSingleLine,QString::fromStdString("(" + std::to_string(m_parameters[i].m_subparameters[j].m_num_shapes) + ")"));
				}

				// SUBINDEX OVERRIDE BUTTON
				{
					// ON STATE
					if(m_parameters[i].m_override){
						// PRESSED
						if(m_button_pressed && m_hover_region == HOVER_REGION_OVERRIDEBUTTON &&  m_current_index == i &&  m_current_subindex == j){
							g_icon_parameterviewer_override_onpressed.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH,y+(size.height()-LAYOUT_OVERRIDEBUTTONWIDTH)/2,LAYOUT_OVERRIDEBUTTONWIDTH,LAYOUT_OVERRIDEBUTTONWIDTH);
						}
						// MOUSE OVER
						else if(m_hover_region == HOVER_REGION_OVERRIDEBUTTON &&  m_current_index == i &&  m_current_subindex == j){
							g_icon_parameterviewer_override_onmouseover.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH,y+(size.height()-LAYOUT_OVERRIDEBUTTONWIDTH)/2,LAYOUT_OVERRIDEBUTTONWIDTH,LAYOUT_OVERRIDEBUTTONWIDTH);
						}
						// NORMAL
						else{
							g_icon_parameterviewer_override_onnormal.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH,y+(size.height()-LAYOUT_OVERRIDEBUTTONWIDTH)/2,LAYOUT_OVERRIDEBUTTONWIDTH,LAYOUT_OVERRIDEBUTTONWIDTH);
						}
					}
					// OFF STATE
					else{
						// PRESSED
						if(m_button_pressed && m_hover_region == HOVER_REGION_OVERRIDEBUTTON &&  m_current_index == i &&  m_current_subindex == j){
							g_icon_parameterviewer_override_offpressed.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH,y+(size.height()-LAYOUT_OVERRIDEBUTTONWIDTH)/2,LAYOUT_OVERRIDEBUTTONWIDTH,LAYOUT_OVERRIDEBUTTONWIDTH);
						}
						// MOUSE OVER
						else if(m_hover_region == HOVER_REGION_OVERRIDEBUTTON &&  m_current_index == i &&  m_current_subindex == j){
							g_icon_parameterviewer_override_offmouseover.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH,y+(size.height()-LAYOUT_OVERRIDEBUTTONWIDTH)/2,LAYOUT_OVERRIDEBUTTONWIDTH,LAYOUT_OVERRIDEBUTTONWIDTH);
						}
						// NORMAL
						else{
							g_icon_parameterviewer_override_offnormal.paint(&painter,LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH,y+(size.height()-LAYOUT_OVERRIDEBUTTONWIDTH)/2,LAYOUT_OVERRIDEBUTTONWIDTH,LAYOUT_OVERRIDEBUTTONWIDTH);
						}
					}
				}

				y += size.height()+LAYOUT_VSPACING;
			}
		}
	}

}

void ParameterViewer::mousePressEvent(QMouseEvent *event)
{
	UNUSED(event);
	switch (m_hover_region) {
		case HOVER_REGION_FOLDBUTTON:{
			if(!m_parameters[m_current_index].m_mergeable){
				if(m_parameters[m_current_index].m_expanded){
					UnexpandParameter(m_current_index);
				}
				else{
					ExpandParameter(m_current_index);
				}
			}
			m_button_pressed = true;
			break;
		}
		case HOVER_REGION_SELECTBUTTON:{
			std::cerr << "SELECT BUTTON PRESSED" << std::endl;
			m_button_pressed = true;
			break;
		}
		case HOVER_REGION_DESELECTBUTTON:{
			std::cerr << "DESELECT BUTTON PRESSED" << std::endl;
			m_button_pressed = true;
			break;
		}
		case HOVER_REGION_OVERRIDEBUTTON:{
			std::cerr << "OVERRIDE BUTTON PRESSED" << std::endl;
			m_button_pressed = true;
			break;
		}
		case HOVER_REGION_NONE:{
			break;
		}
		case HOVER_REGION_LABEL:{
			break;
		}
	}

	if(m_button_pressed){
		viewport()->update();
	}
}

void ParameterViewer::mouseReleaseEvent(QMouseEvent *event)
{
	UNUSED(event);
	if(m_button_pressed){
		m_button_pressed = false;
		viewport()->update();
	}
}


void ParameterViewer::mouseDoubleClickEvent(QMouseEvent *event)
{
	UNUSED(event);
	if(m_hover_region == HOVER_REGION_LABEL){
		if(m_parameters[m_current_index].m_expanded){
			UnexpandParameter(m_current_index);
		}
		else{
			ExpandParameter(m_current_index);
		}
	}

}

void ParameterViewer::mouseMoveEvent(QMouseEvent *event)
{
	changeHoverRegion(getHoverRegion(event->pos()));
}

void ParameterViewer::leaveEvent(QEvent *event)
{
	UNUSED(event);
	m_current_index = INDEX_NONE;
	m_current_subindex = INDEX_NONE;
	m_hover_region = HOVER_REGION_NONE;
	viewport()->update();
}

void ParameterViewer::UpdateFocusChain()
{
	QWidget *current_widget;
	QWidget *next_widget;

	for(index_t i = 0; i < m_parameters.GetSize(); ++i) {

		current_widget = m_parameters[i].m_widget;
		if(!m_parameters[i].m_mergeable && m_parameters[i].m_expanded){
			next_widget = m_parameters[i].m_subparameters[0].m_widget;
			setTabOrder(current_widget,next_widget);

			for(index_t j = 1; j < m_parameters[i].m_subparameters.size(); ++j){
				current_widget = next_widget;
				next_widget = m_parameters[i].m_subparameters[j].m_widget;
				setTabOrder(current_widget,next_widget);
			}
			current_widget = m_parameters[i].m_subparameters.back().m_widget;
		}
		else{
			if(i != m_parameters.GetSize()-1){
				next_widget = m_parameters[i+1].m_widget;
				setTabOrder(current_widget,next_widget);
			}
		}
	}
}

void ParameterViewer::UpdateRange() {
	int height = LAYOUT_VSPACING;
	for(index_t i = 0; i < m_parameters.GetSize(); ++i) {
		QSize size = GetWidgetSize(m_parameters[i].m_widget);
		height += size.height() + LAYOUT_VSPACING;

		if(!m_parameters[i].m_mergeable && m_parameters[i].m_expanded){
			for(index_t j = 0; j < m_parameters[i].m_subparameters.size(); ++j){
				QSize size = GetWidgetSize(m_parameters[i].m_subparameters[j].m_widget);
				height += size.height()+LAYOUT_VSPACING;
			}
		}

	}
	verticalScrollBar()->setPageStep(viewport()->height());
	verticalScrollBar()->setRange(0, std::max(0, height - viewport()->height()));
}

void ParameterViewer::UpdateLayout() {
	int y = -verticalScrollBar()->value();

	y = y + LAYOUT_VSPACING;
	for(index_t i = 0; i < m_parameters.GetSize(); ++i)
	{
		QSize size = GetWidgetSize(m_parameters[i].m_widget);

		m_parameters[i].m_widget->setGeometry(LAYOUT_LABELWIDTH+LAYOUT_FOLDBUTTONWIDTH+4*LAYOUT_HSPACING+LAYOUT_OVERRIDEBUTTONWIDTH, y, viewport()->width()-LAYOUT_LABELWIDTH-LAYOUT_FOLDBUTTONWIDTH-5*LAYOUT_HSPACING-LAYOUT_OVERRIDEBUTTONWIDTH, size.height());
		m_parameters[i].m_widget->setContentsMargins(0,0,0,0);
		if(!m_parameters[i].m_override) {
			static_cast<QLineEdit*>(m_parameters[i].m_widget)->setEnabled(false);
		}

		y += size.height()+LAYOUT_VSPACING;

		if(!m_parameters[i].m_mergeable && m_parameters[i].m_expanded){
			for(index_t j = 0; j < m_parameters[i].m_subparameters.size(); ++j){
				QSize size = GetWidgetSize(m_parameters[i].m_subparameters[j].m_widget);

				m_parameters[i].m_subparameters[j].m_widget->setGeometry(LAYOUT_LABELWIDTH+LAYOUT_FOLDBUTTONWIDTH+4*LAYOUT_HSPACING+LAYOUT_OVERRIDEBUTTONWIDTH, y, viewport()->width()-LAYOUT_LABELWIDTH-LAYOUT_FOLDBUTTONWIDTH-5*LAYOUT_HSPACING-LAYOUT_OVERRIDEBUTTONWIDTH, size.height());
				m_parameters[i].m_subparameters[j].m_widget->setContentsMargins(0,0,0,0);
				y += size.height()+LAYOUT_VSPACING;
			}
		}
	}

	viewport()->update();
}

void ParameterViewer::positionToIndex(const QPoint &pos)
{
	m_current_index = INDEX_NONE;
	m_current_subindex = INDEX_NONE;
	int y = -verticalScrollBar()->value();

	y = y + LAYOUT_VSPACING/2;
	index_t i = 0;
	while(i < m_parameters.GetSize() && m_current_index == INDEX_NONE)
	{
		QSize size = GetWidgetSize(m_parameters[i].m_widget);
		y += size.height()+LAYOUT_VSPACING;

		if(pos.y() < y){
			m_current_index = i;
			break;
		}

		if(!m_parameters[i].m_mergeable && m_parameters[i].m_expanded){
			for(index_t j = 0; j < m_parameters[i].m_subparameters.size(); ++j){
				QSize size = GetWidgetSize(m_parameters[i].m_subparameters[j].m_widget);
				y += size.height()+LAYOUT_VSPACING;

				if(pos.y() < y){
					m_current_index = i;
					m_current_subindex = j;
					break;
				}
			}
		}

		++i;
	}
}

void ParameterViewer::ExpandParameter(index_t index)
{
	if(!m_parameters[index].m_mergeable){
		m_parameters[index].m_expanded = true;

		// TODO replace with shape search
		QLineEdit *widget = new QLineEdit(viewport());
		widget->setText("fgdfg");
		widget->setAutoFillBackground(true);
		widget->setBackgroundRole(QPalette::Window);
		if(widget->focusPolicy() == Qt::NoFocus)
			widget->setFocusPolicy(Qt::ClickFocus);
		widget->show();
		m_parameters[index].m_subparameters.emplace_back(1,widget);

		QLineEdit *widget2 = new QLineEdit(viewport());
		widget2->setText("x");
		widget2->setAutoFillBackground(true);
		widget2->setBackgroundRole(QPalette::Window);
		if(widget2->focusPolicy() == Qt::NoFocus)
			widget2->setFocusPolicy(Qt::ClickFocus);
		widget2->show();
		m_parameters[index].m_subparameters.emplace_back(3,widget2);

		UpdateLayout();
		UpdateFocusChain();
		UpdateRange();
	}
}

void ParameterViewer::UnexpandParameter(index_t index)
{

	if(!m_parameters[index].m_mergeable){
		m_parameters[index].m_expanded = false;
		for(index_t i = 0; i < m_parameters[index].m_subparameters.size(); ++i) {
			m_parameters[index].m_subparameters[i].m_widget->deleteLater();
		}
		m_parameters[index].m_subparameters.clear();

		UpdateLayout();
		UpdateFocusChain();
		UpdateRange();
	}
}

HOVER_REGION ParameterViewer::getHoverRegion(const QPoint &pos)
{
	positionToIndex(pos);
	if(m_current_index != INDEX_NONE && pos.x() < LAYOUT_FOLDBUTTONWIDTH+3*LAYOUT_HSPACING+LAYOUT_LABELWIDTH+LAYOUT_OVERRIDEBUTTONWIDTH){
		// OVERRIDE BUTTON
		if(pos.x() > LAYOUT_FOLDBUTTONWIDTH+3*LAYOUT_HSPACING+LAYOUT_LABELWIDTH){
			return HOVER_REGION_OVERRIDEBUTTON;
		}
		// FOLDBUTTON REGION
		else if(m_current_subindex == INDEX_NONE && pos.x() < LAYOUT_FOLDBUTTONWIDTH+LAYOUT_HSPACING){
			return HOVER_REGION_FOLDBUTTON;
		}
		// LABEL REGION
		else if(m_current_subindex == INDEX_NONE && pos.x() < LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_LABELWIDTH && pos.x() > LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING){
			return HOVER_REGION_LABEL;
		}
		// SELECT BUTTON
		else if(m_current_subindex != INDEX_NONE && pos.x() > LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING && pos.x() < LAYOUT_FOLDBUTTONWIDTH+2*LAYOUT_HSPACING+LAYOUT_SUBPARAMBUTTONWIDTH){
			return HOVER_REGION_SELECTBUTTON;
		}
		// DESELECT BUTTON
		else if(m_current_subindex != INDEX_NONE && pos.x() > LAYOUT_FOLDBUTTONWIDTH+3*LAYOUT_HSPACING+LAYOUT_SUBPARAMBUTTONWIDTH && pos.x() < LAYOUT_FOLDBUTTONWIDTH+3*LAYOUT_HSPACING+2*LAYOUT_SUBPARAMBUTTONWIDTH){
			return HOVER_REGION_DESELECTBUTTON;
		}

		//REST
		else{
			return HOVER_REGION_NONE;
		}
	}
	else{
		return HOVER_REGION_NONE;
	}
}

void ParameterViewer::changeHoverRegion(HOVER_REGION hover_region)
{
	if(m_hover_region != hover_region){
		m_hover_region = hover_region;
		//		viewport()->update(); //TODO only update te viewport when something needs to change
	}
	viewport()->update();
}

void ParameterViewer::ensureWidgetVisible(QWidget *childWidget)
{
	QRect rect = childWidget->geometry().adjusted(0,-LAYOUT_VSPACING,0,LAYOUT_VSPACING);
	if(rect.y() < 0) {
		verticalScrollBar()->setValue(verticalScrollBar()->value() + rect.y());
	}
	if(rect.y() + rect.height() > viewport()->height()) {
		verticalScrollBar()->setValue(verticalScrollBar()->value() + rect.y() + rect.height() - viewport()->height());
	}
}


