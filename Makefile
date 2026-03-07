run:
	@cmake -S . -B build && cd build && make && ./shageki

fmt:
	@clang-format -i src/*

