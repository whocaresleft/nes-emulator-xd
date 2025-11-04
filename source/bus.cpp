#include "../header/bus.h"

#include "../header/cpu.h"
void bus::request_nmi() { return this->_cpu->request_nmi(); }