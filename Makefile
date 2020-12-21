DIR_SRC = src
DIR_BIN = bin

BINS := $(patsubst \
	./${DIR_SRC}/%.c,\
	${DIR_BIN}/%,\
	$(shell find  -name "*.c")\
)

BINDIRS := $(foreach p,${BINS},$(dir $p))

all:  ${BINDIRS} ${BINS}

${DIR_BIN}/% : ${DIR_SRC}/%.c
	gcc $^ -o $@ -g -Wall --pedantic

${BINDIRS}:
	mkdir -p $@
