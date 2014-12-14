/*
 Implicit skinning
 Copyright (C) 2013 Rodolphe Vaillant, Loic Barthe, Florian Cannezin,
 Gael Guennebaud, Marie Paule Cani, Damien Rohmer, Brian Wyvill,
 Olivier Gourmel

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License 3 as published by
 the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>
 */
#include "timer.hpp"

void Timer::start() {
    _boost_timer.restart();
}

double Timer::stop() {
    _elapsed_time = _boost_timer.elapsed();
    return _elapsed_time;
}

void Timer::reset() {
    _elapsed_time = 0.;
    _boost_timer.restart();
}

double Timer::get_value(){
    return _elapsed_time;
}