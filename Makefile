PROGRAM = libsqlite-ulid.so

$(PROGRAM): ulid.c
	gcc -I sqlite/src -fPIC -shared ulid.c -o $(PROGRAM)
	strip $(PROGRAM)
