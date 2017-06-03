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

#include "Drawing.h"

Drawing::Drawing(Library *parent, stringtag_t name, DrawingType type, stringtag_t layerstack) : LibraryTreeItem(LIBRARYTREEITEMTYPE_DRAWING){
	m_parent = parent;
	m_name = name;
	m_type = type;
	m_layerstack = layerstack;

	std::vector<Cow<ShapeInstance>> shapes;
	HistoryClear(std::move(shapes));
}

void Drawing::HistoryClear(std::vector<Cow<ShapeInstance>> &&shapes) {
	m_history.clear();
	m_history.emplace_back(std::move(shapes),false);

	m_history_position = 0;
}

void Drawing::HistoryRevert() {
	//TODO//
}

void Drawing::HistoryPush(std::vector<Cow<ShapeInstance>> &&shapes, bool soft) {
	m_history.resize(m_history_position+1);

	if(soft && m_history.back().IsSoft()) {
		m_history.pop_back();
		m_history.emplace_back(std::move(shapes), soft);
	}
	else {
		m_history.emplace_back(std::move(shapes), soft);

		if(m_history.size() > m_max_history_position) {
			m_history.erase(m_history.begin());
		}
	}

	m_history_position = m_history.size() - 1;
}

void Drawing::HistoryUndo() {
	if(m_history_position > 0) {
		m_history_position = m_history_position - 1;
	}
}

void Drawing::HistoryRedo() {
	if(m_history_position < m_history.size()-1) {
		m_history_position = m_history_position + 1;
	}
}

