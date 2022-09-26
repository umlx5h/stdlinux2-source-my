package main

import (
	"fmt"
	"os"
	"syscall"
)

func main() {
	args := os.Args[1:]
	if len(args) == 0 {
		// if no arguments
		fmt.Fprintf(os.Stderr, "%s: file not given\n", os.Args[0])
		os.Exit(1)
	}

	for _, a := range args {
		doCat(a)
	}

}

func doCat(path string) {
	fd, err := syscall.Open(path, os.O_RDONLY, 0)
	if err != nil {
		die(path, err)
	}

	// buf := make([]byte, 0, 256)
	buf := make([]byte, 256)

LOOP:
	for {
		// n, err := syscall.Read(fd, buf[:cap(buf)])
		n, err := syscall.Read(fd, buf)
		switch {
		case n < 0:
			die(path, err)
		case n == 0:
			break LOOP
			// default:
			// 	buf = buf[:n]
		}
		n, err = syscall.Write(int(os.Stdout.Fd()), buf[:n])
		if n < 0 || err != nil {
			die(path, err)
		}
	}

	if err := syscall.Close(fd); err != nil {
		die(path, err)
	}
}

func die(str string, err error) {
	fmt.Fprintf(os.Stderr, "%s: %s", str, err)
	os.Exit(1)
}
