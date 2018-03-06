env = environment()
env.set('TESTDIR', meson.current_source_dir())
env.set('CHIP8ASM', chip8asm.full_path())
env.set('CHIP8DISASM', chip8disasm.full_path())

check_asm = find_program('check-asm.sh')
test('check-asm', check_asm, env : env)

check_disasm = find_program('check-disasm.sh')
test('check-disasm', check_disasm, env : env)

check_disasm_loop = find_program('check-disasm-loop.sh')
test('check-disasm-loop', check_disasm_loop, env : env)
