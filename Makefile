binary=rmkb
rmdevice?=remarkable.local

.PHONY: clean

${binary}: rmkb.c
	${CC} -O2 -o ${binary} rmkb.c

deploy: ${binary}
	scp ${binary} root@${rmdevice}:${binary}

clean:
	rm ${binary}
