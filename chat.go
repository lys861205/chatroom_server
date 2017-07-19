package main

import (
	"fmt"
	"net"
	"log"
	"bufio"
	"time"
)

type client chan<- string

var (
	entering = make(chan client)
	leaving  = make(chan client)
	messages = make(chan string)
)

func main(){
	listener, err := net.Listen("tcp", "localhost:8000")
	if err != nil {
		log.Fatal(err)
	}

	go broadcaster()

	for{
		conn, err := listener.Accept()
		if err != nil {
			log.Print(err)
			continue
		}
		go handleConn(conn)
	}
}

func broadcaster(){
	clients := make(map[client]bool)
	for {
		select {
		case msg := <- messages:
			for cli := range clients{
				cli <- msg
			}
		case cli := <-entering:
			clients[cli] = true

		case cli := <-leaving:
			delete(clients, cli)
			close(cli)
		}
	}
}

func handleConn(conn net.Conn){
	ch := make(chan string, 4)
	go clientWriter(conn, ch)

	//timer
	tch := make(chan struct{})
	go clientTimer(conn, 5, tch)

	who := conn.RemoteAddr().String()
	ch <- "You are " + who

	messages <- who + " has arrived."

	entering <- ch

	input := bufio.NewScanner(conn)
	for input.Scan() {
		messages <- who + ": " + input.Text()
		tch <- struct{}{}
	}

	tch <- struct{}{}
	leaving <- ch
	messages <- who + " has left."

	conn.Close()
}

func clientWriter(conn net.Conn, ch <-chan string){
	for msg := range ch {
		fmt.Fprintf(conn, msg)
	}
}

func clientTimer(conn net.Conn, n int, ch <-chan struct{} ){
	for {
		fmt.Println(time.Now())
		ticker := time.NewTicker(20* time.Second)
		select {
			case <- ticker.C:
				fmt.Println(time.Now())
				conn.Close()
				return
			case <- ch:
				ticker.Stop()
		}
	}
}