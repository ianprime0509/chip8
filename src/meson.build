chip8_src = [
  'audio.c',
  'chip8.c',
  'instruction.c',
  'interpreter.c',
  'log.c'
]

executable(
  'chip8',
  chip8_src,
  dependencies : sdl,
  include_directories : incdir,
  install : true
)

chip8asm_src = [
  'assembler.c',
  'chip8asm.c',
  'instruction.c',
  'log.c'
]

chip8asm = executable(
  'chip8asm',
  chip8asm_src,
  include_directories : incdir,
  install : true
)

chip8disasm_src = [
  'chip8disasm.c',
  'disassembler.c',
  'instruction.c',
  'log.c'
]

chip8disasm = executable(
  'chip8disasm',
  chip8disasm_src,
  include_directories : incdir,
  install : true
)

chip8test_src = [
  'assembler.c',
  'chip8test.c',
  'instruction.c',
  'interpreter.c',
  'log.c'
]

chip8test = executable(
  'chip8test',
  chip8test_src,
  include_directories : incdir,
)

test('chip8test', chip8test)
