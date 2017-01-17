/*
Copyright (C) 2016  The AlterPCB team
Contact: Maarten Baert <maarten-baert@hotmail.com>

This file is part of AlterPCB.

AlterPCB is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

AlterPCB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this AlterPCB.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <vector>
#include <QtGui>
#include <Shape.h>

class ParameterViewer : public QWidget {
	Q_OBJECT

private:
	std::vector<Shape*> m_selected_shapes;
	QVBoxLayout *m_layout;
	int m_keybox_width;

public:
	ParameterViewer(QWidget *parent = 0);

	inline void SetSelectedShapes(std::vector<Shape*> selected_shapes) {m_selected_shapes = selected_shapes;}

private:
	void DrawData();

};
