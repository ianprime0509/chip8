env = environment()
env.set('TESTDIR', meson.current_source_dir())
env.set('CHIP8ASM', chip8asm.full_path())
env.set('CHIP8DISASM', chip8disasm.full_path())

check_asm = files('check-asm.sh')
test('check-asm', check_asm, env : env)

check_disasm = files('check-disasm.sh')
test('check-disasm', check_disasm, env : env)
