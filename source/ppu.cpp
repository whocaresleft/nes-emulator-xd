#include "../header/ppu.h"

#include "../header/bus.h"
void ppu::request_nmi() { return this->cpu_bus->request_nmi(); }