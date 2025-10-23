#include "cpu.h"

cpu::cpu() :

	opcode(0x00_u8),
	decoded(nullptr),

	pc(0x8000_u16),
	sp(STACK_RESET),
	a(0x00_u8),
	x(0x00_u8),
	y(0x00_u8),
	p(0x00_u8 | F_INTERRUPT_DISABLE | F_GHOST),
	cycles(7_usize),
	halted(false),

	cpu_bus()
{
	//this->pc = this->read_u16(0xFFFC);
}

cpu::cpu(cpu& to_copy) :

	opcode(to_copy.opcode),
	decoded(to_copy.decoded),

	pc(to_copy.pc),
	sp(to_copy.sp),
	a(to_copy.a),
	x(to_copy.x),
	y(to_copy.y),
	p(to_copy.p),
	cycles(to_copy.cycles),
	halted(to_copy.halted),

	cpu_bus(bus{to_copy.cpu_bus})
{ }

cpu::cpu(cpu&& to_move) noexcept :

	opcode(to_move.opcode),
	decoded(std::move(to_move.decoded)),

	pc(to_move.pc),
	sp(to_move.sp),
	a(to_move.a),
	x(to_move.x),
	y(to_move.y),
	p(to_move.p),
	cycles(to_move.cycles),
	halted(to_move.halted),

	cpu_bus(std::move(to_move.cpu_bus))
{ }

cpu::~cpu() { this->cpu_bus.~bus(); }

u8 cpu::read_u8(const u16 address) const {
	return this->cpu_bus.read_u8(address);
}
void cpu::write_u8(const u16 address, const u8 value) {
	this->cpu_bus.write_u8(address, value);
	return;
}

u8 cpu::pop_u8() {
	return this->read_u8(STACK | static_cast<u16>(++this->sp));
}
u16 cpu::pop_u16() {
	return static_cast<u16>(this->pop_u8()) | static_cast<u16>((this->pop_u8()) << 8);
}
void cpu::push_u8(u8 value) {
	this->write_u8(STACK | static_cast<u16>(this->sp--), value);
	return;
}
void cpu::push_u16(u16 value) {
	this->push_u8(static_cast<u8>((value & 0xFF00) >> 8));
	this->push_u8(static_cast<u8>(value & 0x00FF));
	return;
}

std::pair<u16, bool> cpu::get_absolute_address(u16 address) const {
	return std::pair<u16, bool> { this->read_u16(address), false };
}
std::pair<u16, bool> cpu::get_absolute_x_address(u16 address) const {
	u16 base = this->read_u16(address);
	u16 addr = base + static_cast<u16>(this->x);
	return std::pair<u16, bool> { addr, (base & 0xFF00) != (addr & 0xFF00) };
}
std::pair<u16, bool> cpu::get_absolute_y_address(u16 address) const {
	u16 base = this->read_u16(address);
	u16 addr = base + static_cast<u16>(this->y);
	return std::pair<u16, bool> { addr, (base & 0xFF00) != (addr & 0xFF00) };
}
std::pair<u16, bool> cpu::get_immediate_address(u16 address) const {
	return std::pair<u16, bool> { address, false };
}
std::pair<u16, bool> cpu::get_indirect_x_address(u16 address) const {
	u8 base = this->read_u8(address);
	u8 ptr = static_cast<u16>(base + this->x);
	return std::pair<u16, bool> { static_cast<u16>(this->read_u8(ptr)) | (static_cast<u16>(this->read_u8(ptr + 1_u16)) << 8), false };
}
std::pair<u16, bool> cpu::get_indirect_y_address(u16 address) const {
	u8 base = static_cast<u16>(this->read_u8(address));
	u8 lo = static_cast<u16>(this->read_u8(base));
	u8 hi = static_cast<u16>(this->read_u8(base + 1_u16));
	u16 deref_base = (hi << 8) | lo;
	u16 deref = deref_base + static_cast<u16>(this->y);
	return std::pair<u16, bool> {
		deref,
		(deref_base & 0xFF00) != (deref & 0xFF00)
	};
}
std::pair<u16, bool> cpu::get_zero_page_address(u16 address) const {
	return std::pair<u16, bool> { static_cast<u16>(this->read_u8(address)), false };
}
std::pair<u16, bool> cpu::get_zero_page_x_address(u16 address) const {
	return std::pair<u16, bool> {
		static_cast<u16>(this->read_u8(address) + this->x),
		false 
	};
}
std::pair<u16, bool> cpu::get_zero_page_y_address(u16 address) const {
	return std::pair<u16, bool> {
		static_cast<u16>(this->read_u8(address) + this->y),
		false
	};
}

#include "instruction.h"
void cpu::arr (const addressing_mode mode) {
	u16 address((this->*mode.get_address)(this->pc).first);
	u8 value(this->read_u8(address));
	u8 and_(this->a & value);
	u8 carry_in(0b00000000_u8);
	if (this->get_carry()) carry_in = 0b10000000_u8;
	u8 result((and_ >> 1) | carry_in);
	this->set_a(result);
	this->set_carry(0 != (result & 0b01000000) );
	u8 b6((result >> 6) & 1);
	u8 b5((result >> 5) & 1);
	this->set_overflow(0 != (b6 ^ b5));
}
void cpu::jsr (const addressing_mode mode) {
	this->push_u16(this->pc + 1);
	this->pc = ((this->*mode.get_address)(this->pc)).first;
}
void cpu::txa (const addressing_mode mode) {
	this->set_a(this->x);
}
void cpu::stp (const addressing_mode mode) {
	this->halted = true;
}
void cpu::tya (const addressing_mode mode) {
	this->set_a(this->y);
}
void cpu::shy (const addressing_mode mode) {
	u16 address_base = this->read_u16(this->pc);
	u8 lo = static_cast<u8>(address_base & 0x00FF);
	u8 value = this->y & (lo + 1);
	this->write_u8(
		address_base + static_cast<u16>(this->x), value
	);
}
void cpu::slo (const addressing_mode mode) {
	u16 address((this->*mode.get_address)(this->pc).first);
	u8 value = this->read_u8(address);
	this->set_carry((value & 0b10000000_u8) > 0_u8);
	value <<= 1;
	this->write_u8(address, value);
	this->set_a(this->a | value);
}
void cpu::cli (const addressing_mode mode) {
	this->set_interrupt_disable(false);
}
void cpu::lsr (const addressing_mode mode) {
	u16 address((this->*mode.get_address)(this->pc).first);
	u8 value = this->read_u8(address);
	this->set_carry(0 != (value & 0b00000001));
	value >>= 1;
	this->write_u8(address, value);
	this->update_negative_zero(value);
}
void cpu::dcp (const addressing_mode mode) {
	u16 address((this->*mode.get_address)(this->pc).first);
	u8 value = this->read_u8(address) - 1_u8;

	this->write_u8(address, value);
	u8 result(this->a - value);
	this->set_carry(this->a >= value);
	this->update_negative_zero(result);
}
void cpu::cpx (const addressing_mode mode) {
	this->compare(mode, this->x);
}
void cpu::ldx (const addressing_mode mode) {
	std::pair<u16, bool> pair = (this->*mode.get_address)(this->pc);
	this->x = this->read_u8(pair.first);
	this->update_negative_zero(this->x);
	if (pair.second) this->tick(1_usize);
}
void cpu::ror (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 value = this->read_u8(address);

	bool old_carry = this->get_carry();
	this->set_carry((value & 0b00000001) > 0);
	value >>= 1;

	if (old_carry) value |= 0b10000000;
	this->write_u8(address, value);
	this->update_negative_zero(value);
}
void cpu::cmp (const addressing_mode mode) {
	this->compare(mode, this->a);
}
void cpu::and_ (const addressing_mode mode) {
	std::pair<u16, bool> pair = (this->*mode.get_address)(this->pc);
	this->set_a(this->a & this->read_u8(pair.first));
	if (pair.second) this->tick(1);
}
void cpu::isc (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 value = this->read_u8(address) + 1_u8;

	this->write_u8(address, value);
	this->add_to_a(~value);
}
void cpu::bcs (const addressing_mode mode) {
	this->branch(this->get_carry());
}
void cpu::pla (const addressing_mode mode) {
	this->set_a(this->pop_u8());
}
void cpu::bvs (const addressing_mode mode) {
	this->branch(this->get_overflow());
}
void cpu::bit (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 value = this->read_u8(address);

	this->set_zero(0 == (this->a & value));
	this->set_negative((value & 0b10000000) > 0);
	this->set_overflow((value & 0b01000000) > 0);
}
void cpu::axs (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 immediate = this->read_u8(address);

	u8 result = this->a & this->x;
	this->set_carry(result >= immediate);

	this->x = result - immediate;
	this->update_negative_zero(this->x);
}
void cpu::bpl (const addressing_mode mode) {
	this->branch(!this->get_negative());
}
void cpu::ahx (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 hi = static_cast<u8>(address >> 8);
	this->write_u8(address, this->a & this->x & hi);
}
void cpu::cld (const addressing_mode mode) {
	this->set_decimal_mode(false);
}
void cpu::rol_accumulator (const addressing_mode mode) {
	u8 value = this->a;
	bool old_carry = this->get_carry();
	this->set_carry((value & 0b10000000) > 0);

	value <<= 1;
	if (old_carry) value |= 0b00000001;
	this->set_a(value);
}
void cpu::brk (const addressing_mode mode) {
	//this->push_u16(++this->pc);
	//this->push_u8(this->p | F_BREAK);
	//this->set_interrupt_disable(true);
	//this->pc = this->read_u16(0xFFFE);
}
void cpu::bcc (const addressing_mode mode) {
	this->branch(!this->get_carry());
}
void cpu::asl_accumulator (const addressing_mode mode) {
	u8 value = this->a;
	this->set_carry((value & 0b10000000) > 0);
	value <<= 1;
	this->set_a(value);
}
void cpu::sre (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 value = this->read_u8(address);

	this->set_carry((value & 0b00000001) > 0);
	value >>= 1;
	this->write_u8(address, value);
	this->set_a(this->a ^ value);
}
void cpu::pha (const addressing_mode mode) {
	this->push_u8(this->a);
}
void cpu::stx (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	this->write_u8(address, this->x);
}
void cpu::bne (const addressing_mode mode) {
	this->branch(!this->get_zero());
}
void cpu::iny (const addressing_mode mode) {
	this->update_negative_zero(++this->y);
}
void cpu::alr (const addressing_mode mode) {
	u16 address((this->*mode.get_address)(this->pc).first);
	u8 value(this->read_u8(address));
	u8 and_(this->a & value);
	this->set_carry(0 != (and_ & 0b00000001));
	and_ >>= 1;
	this->set_a(and_);
}
void cpu::bvc (const addressing_mode mode) {
	this->branch(!this->get_overflow());
}
void cpu::sec (const addressing_mode mode) {
	this->set_carry(true);
}
void cpu::cpy (const addressing_mode mode) {
	this->compare(mode, this->y);
}
void cpu::plp (const addressing_mode mode) {
	this->p = this->pop_u8() & !F_BREAK;
}
void cpu::rla (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 value = this->read_u8(address);

	u8 carry_in = 0_u8;
	if (this->get_carry()) carry_in = 0b00000001_u8;
	bool carry_out = (value & 0b10000000) > 0;

	this->set_carry(carry_out);

	value = (value << 1) | carry_in;
	this->write_u8(address, value);

	this->set_a(this->a & value);
}
void cpu::tay (const addressing_mode mode) {
	this->y = this->a;
	this->update_negative_zero(this->y);
}
void cpu::lsr_accumulator (const addressing_mode mode) {
	u8 value = this->a;
	this->set_carry(0 != (value & 0b00000001));
	value >>= 1;
	this->set_a(value);
}
void cpu::txs (const addressing_mode mode) {
	this->sp = this->x;
}
void cpu::sbc (const addressing_mode mode) {
	std::pair<u16, bool> pair = (this->*mode.get_address)(this->pc);
	this->add_to_a(this->read_u8(~pair.first));
	if (pair.second) this->tick(1);
}
void cpu::ror_accumulator (const addressing_mode mode) {
	u8 value = this->a;

	bool old_carry = this->get_carry();
	this->set_carry((value & 0b00000001) > 0);
	value >>= 1;

	if (old_carry) value |= 0b10000000;
	this->set_a(value);
}
void cpu::dey (const addressing_mode mode) {
	this->update_negative_zero(--this->y);
}
void cpu::xaa (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	this->set_a(this->x & this->read_u8(address));
}
void cpu::sty (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	this->write_u8(address, this->y);
}
void cpu::adc (const addressing_mode mode) {
	std::pair<u16, bool> pair = (this->*mode.get_address)(this->pc);
	this->add_to_a(this->read_u8(pair.first));
	if (pair.second) this->tick(1);
}
void cpu::php (const addressing_mode mode) {
	this->push_u8(this->p | F_BREAK);
}
void cpu::eor (const addressing_mode mode) {
	std::pair<u16, bool> pair = (this->*mode.get_address)(this->pc);
	this->set_a(this->a ^ this->read_u8(pair.first));
	if (pair.second) this->tick(1);
}
void cpu::sed (const addressing_mode mode) {
	this->set_decimal_mode(true);
}
void cpu::clv (const addressing_mode mode) {
	this->set_overflow(false);
}
void cpu::sei (const addressing_mode mode) {
	this->set_interrupt_disable(true);
}
void cpu::rra (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 value = this->read_u8(address);

	u8 carry_in = 0_u8;
	if (this->get_carry()) carry_in = 0b10000000_u8;
	bool carry_out = (value & 0b00000001) > 0;

	value = (value >> 1) | carry_in;
	this->write_u8(address, value);

	this->set_carry(carry_out);
	this->add_to_a(value);
}
void cpu::rol (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 value = this->read_u8(address);

	bool old_carry = this->get_carry();
	this->set_carry((value & 0b10000000) > 0);

	value <<= 1;
	if (old_carry) value |= 0b00000001;

	this->write_u8(address, value);
	this->update_negative_zero(value);
}
void cpu::jmp_abs (const addressing_mode mode) {
	this->pc = this->read_u16(this->pc);
}
void cpu::dec (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 value = this->read_u8(address) - 1_u8;
	this->write_u8(address, value);
	this->update_negative_zero(value);
}
void cpu::inc (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 value = this->read_u8(address) + 1_u8;
	this->write_u8(address, value);
	this->update_negative_zero(value);
}
void cpu::tax (const addressing_mode mode) {
	this->x = this->a;
	this->update_negative_zero(this->x);
}
void cpu::inx (const addressing_mode mode) {
	this->update_negative_zero(++this->x);
}
void cpu::nop_implied(const addressing_mode mode) {
	// Do nothing
}
void cpu::nop (const addressing_mode mode) {
	std::pair<u16, bool> pair = (this->*mode.get_address)(this->pc);
	this->read_u8(pair.first);
	if (pair.second) this->tick(1);
}
void cpu::clc (const addressing_mode mode) {
	this->set_carry(false);
}
void cpu::tsx (const addressing_mode mode) {
	this->x = this->sp;
	this->update_negative_zero(this->x);
}
void cpu::dex (const addressing_mode mode) {
	this->update_negative_zero(--this->x);
}
void cpu::beq (const addressing_mode mode) {
	this->branch(this->get_zero());
}
void cpu::sax (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	this->write_u8(address, this->a & this->x);
}
void cpu::las (const addressing_mode mode) {
	std::pair<u16, bool> pair = (this->*mode.get_address)(this->pc);
	u8 value = this->read_u8(pair.first);
	this->x = value;
	this->sp = value;
	this->set_a(value);
	if (pair.second) this->tick(1);
}
void cpu::bmi (const addressing_mode mode) {
	this->branch(this->get_negative());
}
void cpu::anc (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 value = this->read_u8(address) & this->a;
	this->set_carry((value & 0b10000000) > 0);
	this->set_a(value);
}
void cpu::asl (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	u8 value = this->read_u8(address);
	this->set_carry((value & 0b10000000) > 0);
	value <<= 1;
	this->write_u8(address, value);
	this->update_negative_zero(value);
}
void cpu::shx (const addressing_mode mode) {
	u16 address_base = this->read_u16(this->pc);
	u8 lo = static_cast<u8>(0x00FF & address_base);
	u8 val = this->x & (lo + 1_u8);
	this->write_u8(
		address_base + static_cast<u16>(this->y), val
	);
}
void cpu::ldy (const addressing_mode mode) {
	std::pair<u16, bool> pair = (this->*mode.get_address)(this->pc);
	this->y = this->read_u8(pair.first);
	this->update_negative_zero(this->y);
	if (pair.second) this->tick(1);
}
void cpu::ora (const addressing_mode mode) {
	std::pair<u16, bool> pair = (this->*mode.get_address)(this->pc);
	this->set_a(this->a | this->read_u8(pair.first));
	if (pair.second) this->tick(1);
}
void cpu::lax (const addressing_mode mode) {
	std::pair<u16, bool> pair = (this->*mode.get_address)(this->pc);
	u8 value = this->read_u8(pair.first);
	this->set_a(value);
	this->x = value;
	this->update_negative_zero(this->x);
	if (pair.second) this->tick(1);
}
void cpu::rts (const addressing_mode mode) {
	this->pc = this->pop_u16() + 1_u16;
}
void cpu::lda (const addressing_mode mode) {
	std::pair<u16, bool> pair = (this->*mode.get_address)(this->pc);
	this->set_a(this->read_u8(pair.first));
	if (pair.second) this->tick(1);
}
void cpu::rti (const addressing_mode mode) {
	this->p = this->pop_u8();
	this->set_break(false);
	this->pc = this->pop_u16();
}
void cpu::tas (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	this->sp = this->a & this->x;
	this->write_u8(address, this->sp & static_cast<u8>(address >> 8));
}
void cpu::sta (const addressing_mode mode) {
	u16 address = (this->*mode.get_address)(this->pc).first;
	this->write_u8(address, this->a);
}
void cpu::jmp_ind (const addressing_mode mode) {
	u16 address = this->read_u16(this->pc);
	u16 indirect_ref(0);

	if (0x00FF == (address & 0x00FF)) {
		indirect_ref = static_cast<u16>(this->read_u8(address)) 
			| (static_cast<u16>(this->read_u8(address & 0xFF00)) << 8);
	}
	else {
		indirect_ref = this->read_u16(address);
	}
	this->pc = indirect_ref;
}

void cpu::set_carry(const bool value)				{ value ? this->p |= F_CARRY : this->p &= ~F_CARRY; }
void cpu::set_zero(const bool value)				{ value ? this->p |= F_ZERO : this->p &= ~F_ZERO; }
void cpu::set_interrupt_disable(const bool value)	{ value ? this->p |= F_INTERRUPT_DISABLE : this->p &= ~F_INTERRUPT_DISABLE; }
void cpu::set_decimal_mode(const bool value)		{ value ? this->p |= F_DECIMAL_MODE : this->p &= ~F_DECIMAL_MODE; }
void cpu::set_break(const bool value)				{ value ? this->p |= F_BREAK : this->p &= ~F_BREAK; }
void cpu::set_overflow(const bool value)			{ value ? this->p |= F_OVERFLOW : this->p &= ~F_OVERFLOW; }
void cpu::set_negative(const bool value)			{ value ? this->p |= F_NEGATIVE : this->p &= ~F_NEGATIVE; }

bool cpu::get_carry()				{ return 0 != (this->p & F_CARRY); }
bool cpu::get_zero()				{ return 0 != (this->p & F_ZERO); }
bool cpu::get_interrupt_disable()	{ return 0 != (this->p & F_INTERRUPT_DISABLE); }
bool cpu::get_decimal_mode()		{ return 0 != (this->p & F_DECIMAL_MODE); }
bool cpu::get_break()				{ return 0 != (this->p & F_BREAK); }
bool cpu::get_overflow()			{ return 0 != (this->p & F_OVERFLOW); }
bool cpu::get_negative()			{ return 0 != (this->p & F_NEGATIVE); }

void cpu::update_negative_zero(const u8 value) {
	this->set_zero(value == 0_u8);
	this->set_negative(0_u8 != (value & 0b10000000));
	return;
}

void cpu::tick(const usize cycles) {
	this->cycles += cycles;
}

void cpu::set_a(const u8 value) {
	this->a = value;
	this->update_negative_zero(this->a);
}
void cpu::add_to_a(const u8 value) {
	u16 carry;
	if (this->get_carry()) carry = 1_u16;
	else carry = 0_u16;
	u16 sum = static_cast<u16>(this->a) + static_cast<u16>(value) + carry;

	this->set_carry(sum > 0x00FF);
	u8 result = static_cast<u8>(sum & 0x00FF);

	this->set_overflow(0 != ((value ^ result) & (result ^ this->a) & (0x80_u8)));
	this->set_a(result);
}
void cpu::branch(const bool condition) {
	if (!condition) return;

	u16 old_pc = this->pc;

	i8 jump = static_cast<i8>(this->read_u8(this->pc));
	this->pc = this->pc + 1_u16 + static_cast<i16>(jump);

	this->tick(1);
	if ((old_pc & 0xFF00) != (this->pc & 0xFF00)) this->tick(1);
}
void cpu::compare(const addressing_mode mode, const u8 compare_with) {
	std::pair<u16, bool> addr_page_cross = (this->*mode.get_address)(this->pc);
	u8 value = this->read_u8(addr_page_cross.first);
	this->set_carry(value <= compare_with);
	this->update_negative_zero(compare_with - value);
	if (addr_page_cross.second) this->tick(1);
}

void cpu::fetch() {
	this->opcode = this->read_u8(this->pc++);
}

void cpu::decode() {
	this->decoded = &INSTRUCTIONS[static_cast<usize>(this->opcode)];
}

void cpu::execute() {
	const instruction* instruction = this->decoded;
	if (instruction == nullptr) { return; }
	this->decoded = nullptr;

	addressing_mode mode = ADDRESSING_MODES[instruction->mode];

	u16 old_pc = this->pc;
	(this->*instruction->handler)(mode);

	this->tick(instruction->cycles);
	if (old_pc == this->pc) { this->pc = this-> pc + static_cast<u16>(mode.length - 1_u8); }
	return;
}
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
std::string cpu::trace() const {

	u16 pc = this->pc;
	u8 opcode = this->read_u8(pc);
	u16 begin = pc;

	const auto& instr = INSTRUCTIONS[opcode];
	const addressing_mode mode = ADDRESSING_MODES[instr.mode];

	u16 mem_addr;
	u8 stored_value;
	switch (instr.mode) {
	
	case accumulator:
	case implied:
	case immediate:
	case indirect:
	case relative:
		mem_addr = 0;
		stored_value = 0;
		break;
		
	case absolute:
	case absolute_x:
	case absolute_y:
	case zero_page:
	case zero_page_x:
	case zero_page_y:
	case indirect_x:
	case indirect_y:
		mem_addr = (this->*mode.get_address)(begin + 1).first;
		stored_value = this->read_u8(mem_addr);
		break;
	default:
		throw std::runtime_error("Impossible");
	}

	std::ostringstream hex;
	u8 pc_p_1(0), pc_p_2(0);

	hex << std::hex << std::uppercase << std::setfill('0');
	hex << std::setw(2) << int(instr.opcode);

	if (mode.length > 1) {
		pc_p_1 = this->read_u8(begin + 1);
		hex << " " << std::setw(2) << int(pc_p_1);
	}
	else { hex << "   "; }
	if (mode.length > 2) {
		pc_p_2 = this->read_u8(begin + 2);
		hex << " " << std::setw(2) << int(pc_p_2);
	}
	else { hex << "   "; }

	std::ostringstream disassembled;
	disassembled << std::hex << std::uppercase << std::setfill('0');
	switch (instr.mode) {
	case implied:
		disassembled << instr.mnemonic;
		break;
	case accumulator:
		disassembled << instr.mnemonic << " A";
		break;
	case immediate:
		disassembled << instr.mnemonic << " #$" << std::setw(2) << int(pc_p_1);
		break;
	case zero_page:
		disassembled << instr.mnemonic << " $" << std::setw(2) << int(mem_addr) << " = " << std::setw(2) << int(stored_value);
		break;
	case zero_page_x:
		disassembled << instr.mnemonic << " $" << std::setw(2) << int(pc_p_1) << ",X @ " << std::setw(4) << int(mem_addr) << " = " << std::setw(2) << int(stored_value);
		break;
	case zero_page_y:
		disassembled << instr.mnemonic << " $" << std::setw(2) << int(pc_p_1) << ",Y @ " << std::setw(4) << int(mem_addr) << " = " << std::setw(2) << int(stored_value);
		break;
	case relative:
		disassembled << instr.mnemonic << " $" << std::setw(4) << int((static_cast<u16>(static_cast<u16>(begin + 2) + static_cast<i8>(pc_p_1))));
		break;
	case indirect_x:
		disassembled << instr.mnemonic << " ($" << std::setw(2) << int(pc_p_1) << ",X) @ " << int(pc_p_1 + this->x) << " = " << std::setw(4) << std::setw(4) << int(mem_addr) << " = " << int(stored_value);
		break;
	case indirect_y:
		disassembled << instr.mnemonic << " ($" << std::setw(2) << int(pc_p_1) << "),Y = " << std::setw(4) << int(mem_addr - static_cast<u16>(this->y)) << " @ " << int(mem_addr) << " = " << int(stored_value);
		break;
	case absolute:
		disassembled << instr.mnemonic << " $" << std::setw(4) << int(mem_addr) << " = " << std::setw(2) << int(stored_value);
		break;
	case absolute_x:
		disassembled << instr.mode << " $" << std::setw(4) << int((static_cast<u16>(pc_p_2) << 8) | static_cast<u16>(pc_p_1)) << ",X @ " << std::setw(4) << int(mem_addr) << " = " << std::setw(2) << int(stored_value);
		break;
	case absolute_y:
		disassembled << instr.mode << " $" << std::setw(4) << int((static_cast<u16>(pc_p_2) << 8) | static_cast<u16>(pc_p_1)) << ",Y @ " << std::setw(4) << int(mem_addr) << " = " << std::setw(2) << int(stored_value);
		break;
	case indirect:
		u16 addr = ((static_cast<u16>(pc_p_2) << 8) | static_cast<u16>(pc_p_1));
		u16 jump_addr = ((addr & 0x00FF) == 0x00FF) 
			? ((static_cast<u16>(this->read_u8(addr & 0xFF00)) << 8) | static_cast<u16>(this->read_u8(addr)))
			: this->read_u16(addr);
		disassembled << instr.mnemonic << " ($" << std::setw(4) << int(addr) << ") = " << std::setw(4) << int(jump_addr);
		break;
	}

	std::ostringstream result;
	result << std::uppercase << std::hex
		<< std::setw(4) << pc << "  "
		<< hex.str() << "  "
		<< std::left << std::setw(31) << disassembled.str()
		<< std::setfill('0')
		<< " A:" << std::setw(2) << int(this->a) << " "
		<< " X:" << std::setw(2) << int(this->x) << " "
		<< " Y:" << std::setw(2) << int(this->y) << " "
		<< " P:" << std::setw(2) << int(this->p) << " "
		<< " SP:" << std::setw(2) << int(this->sp) << " "
		<< std::dec << "CYC: " << this->cycles;
	return result.str();
}

void cpu::load(cartridge* rom) {
	this->cpu_bus.load(rom);
}
void cpu::reset() {
	this->opcode = 0x00_u8;
	this->decoded = nullptr;
	this->sp = STACK_RESET;
	this->a = 0x00_u8;
	this->x = 0x00_u8;
	this->y = 0x00_u8;
	this->p = 0x00_u8 | F_INTERRUPT_DISABLE | F_GHOST;
	this->cycles = 7_usize;
	this->halted = false;

	//this->pc = this->read_u16(0xFFFC_u16);
	this->pc = 0xC000;
}

void cpu::run() {
	this->run_with_callbacks(
		[](cpu&) {},
		[](cpu&) {}
	);
}
constexpr addressing_mode ADDRESSING_MODES[13] = {
	{ 3_u8, &cpu::get_absolute_address }, // absolute
	{ 3_u8, &cpu::get_absolute_x_address }, // absolute_x
	{ 3_u8, &cpu::get_absolute_y_address }, // absolute_y
	{ 1_u8, nullptr }, // accumulator
	{ 2_u8, &cpu::get_immediate_address }, // immediate
	{ 1_u8, nullptr }, // implied, 0 bc its instruction specific
	{ 3_u8, nullptr }, // indirect
	{ 2_u8, &cpu::get_indirect_x_address }, // indirect_x
	{ 2_u8, &cpu::get_indirect_y_address }, // indirect_y
	{ 2_u8, nullptr }, // relative
	{ 2_u8, &cpu::get_zero_page_address }, // zero_page
	{ 2_u8, &cpu::get_zero_page_x_address }, // zero_page_x
	{ 2_u8, &cpu::get_zero_page_y_address }, // zero_page_y
};
constexpr instruction INSTRUCTIONS[0x100] = {
	{ 0x00, "BRK", implied,		7, &cpu::brk },
	{ 0x01, "ORA", indirect_x,	6, &cpu::ora },
	{ 0x02, "STP", implied,		1, &cpu::stp },
	{ 0x03, "SLO", indirect_x,  8, &cpu::slo },
	{ 0x04, "NOP", zero_page,	3, &cpu::nop },
	{ 0x05, "ORA", zero_page,	5, &cpu::ora },
	{ 0x06, "ASL", zero_page,	5, &cpu::asl },
	{ 0x07, "SLO", zero_page,   5, &cpu::slo },
	{ 0x08, "PHP", implied,		4, &cpu::php },
	{ 0x09, "ORA", immediate,	2, &cpu::ora },
	{ 0x0A, "ASL", accumulator, 2, &cpu::asl_accumulator },
	{ 0x0B, "ANC", immediate,   2, &cpu::anc },
	{ 0x0C, "NOP", absolute,    4, &cpu::nop },
	{ 0x0D, "ORA", absolute,	4, &cpu::ora },
	{ 0x0E, "ASL", absolute,	6, &cpu::asl },
	{ 0x0F, "SLO", absolute,    6, &cpu::slo },
	{ 0x10, "BPL", relative,	2, &cpu::bpl },
	{ 0x11, "ORA", indirect_y,	5, &cpu::ora },
	{ 0x12, "STP", implied,		1, &cpu::stp },
	{ 0x13, "SLO", indirect_y,  8, &cpu::slo },
	{ 0x14, "NOP", zero_page_x, 4, &cpu::nop },
	{ 0x15, "ORA", zero_page_x,	4, &cpu::ora },
	{ 0x16, "ASL", zero_page_x, 6, &cpu::asl },
	{ 0x17, "SLO", zero_page_x, 6, &cpu::slo },
	{ 0x18, "CLC", implied,		2, &cpu::clc },
	{ 0x19, "ORA", absolute_y,	4, &cpu::ora },
	{ 0x1A, "NOP", implied,		2, &cpu::nop_implied },
	{ 0x1B, "SLO", absolute_y,  7, &cpu::slo },
	{ 0x1C, "NOP", absolute_x,  4, &cpu::nop },
	{ 0x1D, "ORA", absolute_x,	4, &cpu::ora },
	{ 0x1E, "ASL", absolute_x,  7, &cpu::asl },
	{ 0x1F, "SLO", absolute_x,  7, &cpu::slo },
	{ 0x20, "JSR", absolute,	6, &cpu::jsr },
	{ 0x21, "AND", indirect_x,	6, &cpu::and_ },
	{ 0x22, "STP", implied,		1, &cpu::stp },
	{ 0x23, "RLA", indirect_x,  8, &cpu::rla },
	{ 0x24, "BIT", zero_page,	3, &cpu::bit },
	{ 0x25, "AND", zero_page,	3, &cpu::and_ },
	{ 0x26, "ROL", zero_page,	5, &cpu::rol },
	{ 0x27, "RLA", zero_page,   5, &cpu::rla },
	{ 0x28, "PLP", implied,		4, &cpu::plp },
	{ 0x29, "AND", immediate,	2, &cpu::and_ },
	{ 0x2A, "ROL", accumulator, 2, &cpu::rol_accumulator },
	{ 0x2B, "ANC", immediate,   2, &cpu::anc },
	{ 0x2C, "BIT", absolute,	4, &cpu::bit },
	{ 0x2D, "AND", absolute,	4, &cpu::and_ },
	{ 0x2E, "ROL", absolute,	6, &cpu::rol },
	{ 0x2F, "RLA", absolute,    6, &cpu::rla },
	{ 0x30, "BMI", relative,	2, &cpu::bmi },
	{ 0x31, "AND", indirect_y,	5, &cpu::and_ },
	{ 0x32, "STP", implied,		1, &cpu::stp },
	{ 0x33, "RLA", indirect_y,  8, &cpu::rla },
	{ 0x34, "NOP", zero_page_x, 4, &cpu::nop },
	{ 0x35, "AND", zero_page_x, 4, &cpu::and_ },
	{ 0x36, "ROL", zero_page_x, 6, &cpu::rol },
	{ 0x37, "RLA", zero_page_x, 6, &cpu::rla },
	{ 0x38, "SEC", implied,		2, &cpu::sec },
	{ 0x39, "AND", absolute_y,	4, &cpu::and_ },
	{ 0x3A, "NOP", implied,		2, &cpu::nop_implied },
	{ 0x3B, "RLA", absolute_y,  7, &cpu::rla },
	{ 0x3C, "NOP", absolute_x,  4, &cpu::nop },
	{ 0x3D, "AND", absolute_x,	4, &cpu::and_ },
	{ 0x3E, "ROL", absolute_x,	7, &cpu::rol },
	{ 0x3F, "RLA", absolute_x,  7, &cpu::rla },
	{ 0x40, "RTI", implied,		6, &cpu::rti },
	{ 0x41, "EOR", indirect_x,	6, &cpu::eor },
	{ 0x42, "STP", implied,		1, &cpu::stp },
	{ 0x43, "SRE", indirect_x,  8, &cpu::sre },
	{ 0x44, "NOP", zero_page,   3, &cpu::nop },
	{ 0x45, "EOR", zero_page,	3, &cpu::eor },
	{ 0x46, "LSR", zero_page,	5, &cpu::lsr },
	{ 0x47, "SRE", zero_page,   5, &cpu::sre },
	{ 0x48, "PHA", implied,		3, &cpu::pha },
	{ 0x49, "EOR", immediate,	2, &cpu::eor },
	{ 0x4A, "LSR", accumulator, 2, &cpu::lsr_accumulator },
	{ 0x4B, "ALR", immediate,   2, &cpu::alr },
	{ 0x4C, "JMP", absolute,	3, &cpu::jmp_abs },
	{ 0x4D, "EOR", absolute,	4, &cpu::eor },
	{ 0x4E, "LSR", absolute,	6, &cpu::lsr },
	{ 0x4F, "SRE", absolute,    6, &cpu::sre },
	{ 0x50, "BVC", relative,	2, &cpu::bvc },
	{ 0x51, "EOR", indirect_y,	5, &cpu::eor },
	{ 0x52, "STP", implied,		1, &cpu::stp },
	{ 0x53, "SRE", indirect_y,  8, &cpu::sre },
	{ 0x54, "NOP", zero_page_x, 4, &cpu::nop },
	{ 0x55, "EOR", zero_page_x, 4, &cpu::eor },
	{ 0x56, "LSR", zero_page_x, 6, &cpu::lsr },
	{ 0x57, "SRE", zero_page_x, 6, &cpu::sre },
	{ 0x58, "CLI", implied,		2, &cpu::cli },
	{ 0x59, "EOR", absolute_y,	4, &cpu::eor },
	{ 0x5A, "NOP", implied,		2, &cpu::nop_implied },
	{ 0x5B, "SRE", absolute_y,  7, &cpu::sre },
	{ 0x5C, "NOP", absolute_x,  4, &cpu::nop },
	{ 0x5D, "EOR", absolute_x,	4, &cpu::eor },
	{ 0x5E, "LSR", absolute_x,	7, &cpu::lsr },
	{ 0x5F, "SRE", absolute_x,  7, &cpu::sre },
	{ 0x60, "RTS", implied,		6, &cpu::rts },
	{ 0x61, "ADC", indirect_x,	6, &cpu::adc },
	{ 0x62, "STP", implied,		1, &cpu::stp },
	{ 0x63, "RRA", indirect_x,  8, &cpu::rra },
	{ 0x64, "NOP", zero_page,   3, &cpu::nop },
	{ 0x65, "ADC", zero_page,	3, &cpu::adc },
	{ 0x66, "ROR", zero_page,	5, &cpu::ror },
	{ 0x67, "RRA", zero_page,   5, &cpu::rra },
	{ 0x68, "PLA", implied,		4, &cpu::pla },
	{ 0x69, "ADC", immediate,	2, &cpu::adc },
	{ 0x6A, "ROR", accumulator, 2, &cpu::ror_accumulator },
	{ 0x6B, "ARR", immediate,   2, &cpu::arr },
	{ 0x6C, "JMP", indirect,	5, &cpu::jmp_ind },
	{ 0x6D, "ADC", absolute,	4, &cpu::adc },
	{ 0x6E, "ROR", absolute,	6, &cpu::ror },
	{ 0x6F, "RRA", absolute,    6, &cpu::rra },
	{ 0x70, "BVS", relative,	2, &cpu::bvs },
	{ 0x71, "ADC", indirect_y,	5, &cpu::adc },
	{ 0x72, "STP", implied,		1, &cpu::stp },
	{ 0x73, "RRA", indirect_y,  8, &cpu::rra },
	{ 0x74, "NOP", zero_page_x, 4, &cpu::nop },
	{ 0x75, "ADC", zero_page_x, 4, &cpu::adc },
	{ 0x76, "ROR", zero_page_x, 6, &cpu::ror },
	{ 0x77, "RRA", zero_page_x, 6, &cpu::rra },
	{ 0x78, "SEI", implied,		2, &cpu::sei },
	{ 0x79, "ADC", absolute_y,	4, &cpu::adc },
	{ 0x7A, "NOP", implied,		2, &cpu::nop_implied },
	{ 0x7B, "RRA", absolute_y,  7, &cpu::rra },
	{ 0x7C, "NOP", absolute_x,  4, &cpu::nop },
	{ 0x7D, "ADC", absolute_x,	4, &cpu::adc },
	{ 0x7E, "ROR", absolute_x,	7, &cpu::ror },
	{ 0x7F, "RRA", absolute_x,  7, &cpu::rra },
	{ 0x80, "NOP", immediate,   2, &cpu::nop },
	{ 0x81, "STA", indirect_x,  6, &cpu::sta },
	{ 0x82, "NOP", immediate,   2, &cpu::nop },
	{ 0x83, "SAX", indirect_x,  6, &cpu::sax },
	{ 0x84, "STY", zero_page,	3, &cpu::sty },
	{ 0x85, "STA", zero_page,	3, &cpu::sta },
	{ 0x86, "STX", zero_page,	3, &cpu::stx },
	{ 0x87, "SAX", zero_page,   3, &cpu::sax },
	{ 0x88, "DEY", implied,		2, &cpu::dey },
	{ 0x89, "NOP", immediate,   2, &cpu::nop },
	{ 0x8A, "TXA", implied,		2, &cpu::txa },
	{ 0x8B, "XAA", immediate,   2, &cpu::xaa },
	{ 0x8C, "STY", absolute,	4, &cpu::sty },
	{ 0x8D, "STA", absolute,	4, &cpu::sta },
	{ 0x8E, "STX", absolute,	4, &cpu::stx },
	{ 0x8F, "SAX", absolute,    4, &cpu::sax },
	{ 0x90, "BCC", relative,	2, &cpu::bcc },
	{ 0x91, "STA", indirect_y,	6, &cpu::sta },
	{ 0x92, "STP", implied,		1, &cpu::stp },
	{ 0x93, "AHX", indirect_y,  6, &cpu::ahx },
	{ 0x94, "STY", zero_page_x, 4, &cpu::sty },
	{ 0x95, "STA", zero_page_x, 4, &cpu::sta },
	{ 0x96, "STX", zero_page_y, 4, &cpu::stx },
	{ 0x97, "SAX", zero_page_y, 4, &cpu::sax },
	{ 0x98, "TYA", implied,		2, &cpu::tya },
	{ 0x99, "STA", absolute_y,	5, &cpu::sta },
	{ 0x9A, "TXS", implied,		2, &cpu::txs },
	{ 0x9B, "TAS", absolute_y,  5, &cpu::tas },
	{ 0x9C, "SHY", absolute_x,  5, &cpu::shy },
	{ 0x9D, "STA", absolute_x,	5, &cpu::sta },
	{ 0x9E, "SHX", absolute_y,  5, &cpu::shx },
	{ 0x9F, "AHX", absolute_y,  5, &cpu::ahx },
	{ 0xA0, "LDY", immediate,	2, &cpu::ldy },
	{ 0xA1, "LDA", indirect_x,	6, &cpu::lda },
	{ 0xA2, "LDX", immediate,	2, &cpu::ldx },
	{ 0xA3, "LAX", indirect_x,  6, &cpu::lax },
	{ 0xA4, "LDY", zero_page,	3, &cpu::ldy },
	{ 0xA5, "LDA", zero_page,	3, &cpu::lda },
	{ 0xA6, "LDX", zero_page,	3, &cpu::ldx },
	{ 0xA7, "LAX", zero_page,   3, &cpu::lax },
	{ 0xA8, "TAY", implied,		2, &cpu::tay },
	{ 0xA9, "LDA", immediate,	2, &cpu::lda },
	{ 0xAA, "TAX", implied,		2, &cpu::tax },
	{ 0xAB, "LAX", immediate,   2, &cpu::lax },
	{ 0xAC, "LDY", absolute,	4, &cpu::ldy },
	{ 0xAD, "LDA", absolute,	4, &cpu::lda },
	{ 0xAE, "LDX", absolute,	4,  &cpu::ldx },
	{ 0xAF, "LAX", absolute,    4, &cpu::lax },
	{ 0xB0, "BCS", relative,	2, &cpu::bcs },
	{ 0xB1, "LDA", indirect_y,	5, &cpu::lda },
	{ 0xB2, "STP", implied,		1, &cpu::stp },
	{ 0xB3, "LAX", indirect_y,  5, &cpu::lax },
	{ 0xB4, "LDY", zero_page_x, 4, &cpu::ldy },
	{ 0xB5, "LDA", zero_page_x, 4, &cpu::lda },
	{ 0xB6, "LDX", zero_page_y, 4, &cpu::ldx },
	{ 0xB7, "LAX", zero_page_y, 4, &cpu::lax },
	{ 0xB8, "CLV", implied,		2, &cpu::clv },
	{ 0xB9, "LDA", absolute_y,	4, &cpu::lda },
	{ 0xBA, "TSX", implied,		2, &cpu::tsx },
	{ 0xBB, "LAS", absolute_y,  4, &cpu::las },
	{ 0xBC, "LDY", absolute_x,	4, &cpu::ldy },
	{ 0xBD, "LDA", absolute_x,	4, &cpu::lda },
	{ 0xBE, "LDX", absolute_y,	4, &cpu::ldx },
	{ 0xBF, "LAX", absolute_y,  4, &cpu::lax },
	{ 0xC0, "CPY", immediate,	2, &cpu::cpy },
	{ 0xC1, "CMP", indirect_x,	6, &cpu::cmp },
	{ 0xC2, "NOP", immediate,   2, &cpu::nop },
	{ 0xC3, "DCP", indirect_x,  8, &cpu::dcp },
	{ 0xC4, "CPY", zero_page,	3, &cpu::cpy },
	{ 0xC5, "CMP", zero_page,	3, &cpu::cmp },
	{ 0xC6, "DEC", zero_page,	5, &cpu::dec },
	{ 0xC7, "DCP", zero_page,   5, &cpu::dcp },
	{ 0xC8, "INY", implied,		2, &cpu::iny },
	{ 0xC9, "CMP", immediate,	2, &cpu::cmp },
	{ 0xCA, "DEX", implied,		2, &cpu::dex },
	{ 0xCB, "AXS", immediate,   2, &cpu::axs },
	{ 0xCC, "CPY", absolute,	4, &cpu::cpy },
	{ 0xCD, "CMP", absolute,	4, &cpu::cmp },
	{ 0xCE, "DEC", absolute,	6, &cpu::dec },
	{ 0xCF, "DCP", absolute,    6, &cpu::dcp },
	{ 0xD0, "BNE", relative,	2, &cpu::bne },
	{ 0xD1, "CMP", indirect_y,	5, &cpu::cmp },
	{ 0xD2, "STP", implied,		1, &cpu::stp },
	{ 0xD3, "DCP", indirect_y,  8, &cpu::dcp },
	{ 0xD4, "NOP", zero_page_x, 4, &cpu::nop },
	{ 0xD5, "CMP", zero_page_x, 4, &cpu::cmp },
	{ 0xD6, "DEC", zero_page_x, 6, &cpu::dec },
	{ 0xD7, "DCP", zero_page_x, 6, &cpu::dcp },
	{ 0xD8, "CLD", implied,		2, &cpu::cld },
	{ 0xD9, "CMP", absolute_y,	4, &cpu::cmp },
	{ 0xDA, "NOP", implied,		2, &cpu::nop_implied },
	{ 0xDB, "DCP", absolute_y,  7, &cpu::dcp },
	{ 0xDC, "NOP", absolute_x,  4, &cpu::nop },
	{ 0xDD, "CMP", absolute_x,	4, &cpu::cmp },
	{ 0xDE, "DEC", absolute_x,	7, &cpu::dec },
	{ 0xDF, "DCP", absolute_x,  7, &cpu::dcp },
	{ 0xE0, "CPX", immediate,	2, &cpu::cpx },
	{ 0xE1, "SBC", indirect_x,	6, &cpu::sbc },
	{ 0xE2, "NOP", implied,     2, &cpu::nop_implied },
	{ 0xE3, "ISC", indirect_x,  8, &cpu::isc },
	{ 0xE4, "CPX", zero_page,	3, &cpu::cpx },
	{ 0xE5, "SBC", zero_page,	3, &cpu::sbc },
	{ 0xE6, "INC", zero_page,	5, &cpu::inc },
	{ 0xE7, "ISC", zero_page,   5, &cpu::isc },
	{ 0xE8, "INX", implied,		2, &cpu::inx },
	{ 0xE9, "SBC", immediate,	2, &cpu::sbc },
	{ 0xEA, "NOP", implied,		2, &cpu::nop_implied },
	{ 0xEB, "SBC", immediate,   2, &cpu::sbc },
	{ 0xEC, "CPX", absolute,	4, &cpu::cpx },
	{ 0xED, "SBC", absolute,	4, &cpu::sbc },
	{ 0xEE, "INC", absolute,	6, &cpu::inc },
	{ 0xEF, "ISC", absolute,    6, &cpu::isc },
	{ 0xF0, "BEQ", relative,	2, &cpu::beq },
	{ 0xF1, "SBC", indirect_y,	5, &cpu::sbc },
	{ 0xF2, "STP", implied,		1, &cpu::stp },
	{ 0xF3, "ISC", indirect_y,  8, &cpu::isc },
	{ 0xF4, "NOP", zero_page_x, 4, &cpu::nop },
	{ 0xF5, "SBC", zero_page_x, 4, &cpu::sbc },
	{ 0xF6, "INC", zero_page_x, 6, &cpu::inc },
	{ 0xF7, "ISC", zero_page_x, 6, &cpu::isc },
	{ 0xF8, "SED", implied,		2, &cpu::sed },
	{ 0xF9, "SBC", absolute_y,	4, &cpu::sbc },
	{ 0xFA, "NOP", implied,		2, &cpu::nop_implied },
	{ 0xFB, "ISC", absolute_y,  7, &cpu::isc },
	{ 0xFC, "NOP", absolute_x,  4, &cpu::nop },
	{ 0xFD, "SBC", absolute_x,	4, &cpu::sbc },
	{ 0xFE, "INC", absolute_x,	7, &cpu::inc },
	{ 0xFF, "ISC", absolute_x,	7, &cpu::isc },
};