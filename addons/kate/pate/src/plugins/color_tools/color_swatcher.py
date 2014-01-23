# -*- coding: utf-8 -*-
#
# Kate/Pâté color plugins
# Copyright 2013 by Alex Turbov <i.zaufi@gmail.com>
# Copyright 2013 by Phil Schaf
#
#
# This software is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this software.  If not, see <http://www.gnu.org/licenses/>.
#

from PyQt4.QtCore import QTimer
from PyQt4.QtGui import QColor, QPalette, QToolTip

import kate


class ColorSwatcher:
    '''Class encapsuling the ability to show color swatches (colored tooltips)'''
    _SWATCH_TEMPLATE = '<div>{}</div>'.format('&nbsp;' * 10)

    def __init__(self):
        self.old_palette = None
        kate.viewChanged(self.view_changed)
        self.view_changed()


    def view_changed(self):
        '''Connects a swatch showing slot to each view’s selection change signal'''
        view = kate.activeView()
        if view:
            view.selectionChanged.connect(self.show_swatch)


    def show_swatch(self, view):
        '''Shows the swatch if a valid color is selected'''
        if view.selection():
            color = QColor(view.selectionText())
            if color.isValid():
                cursor_pos = view.cursorPositionCoordinates()
                QToolTip.showText(cursor_pos, ColorSwatcher._SWATCH_TEMPLATE)
                self.change_palette(color)


    def change_palette(self, color):
        '''Sets the global tooltip background to the given color and initializes reset'''
        self.old_palette = QToolTip.palette()
        p = QPalette(self.old_palette)
        p.setColor(QPalette.All, QPalette.ToolTipBase, color)
        QToolTip.setPalette(p)

        self.timer = QTimer()
        self.timer.timeout.connect(self.try_reset_palette)
        self.timer.start(300) #short enough not to flicker a wrongly colored tooltip


    def try_reset_palette(self):
        '''Resets the global tooltip background color as soon as the swatch is hidden'''
        if self.old_palette is not None and not QToolTip.isVisible():
            QToolTip.setPalette(self.old_palette)
            self.old_palette = None
