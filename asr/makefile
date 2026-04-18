Target=asr
Object=alsa.o main.o sherpa.o kws.o

CFLAGS=-I/home/sherpa/sherpa-onnx-v1.12.1-linux-aarch64-jni/include/

LDLIBS=-lasound -lonnxruntime -lsherpa-onnx-c-api -L/home/sherpa/sherpa-onnx-v1.12.1-linux-aarch64-shared-cpu/lib

$(Target):$(Object)
	gcc $(Object) -o $(Target) $(LDLIBS)

clean:
	rm -f $(Target) *.o
