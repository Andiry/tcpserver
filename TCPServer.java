import java.io.*;
import java.net.*;

class TCPServer {
	public static void main(String argv[]) throws Exception
	{
		String sentence;
		String modified;
		ServerSocket welcomeSocket = new ServerSocket(6789);
		while(true) {
			Socket connSocket = welcomeSocket.accept();
			BufferedReader inFromClient = new BufferedReader(new InputStreamReader(connSocket.getInputStream()));
			DataOutputStream outToClient = new DataOutputStream(connSocket.getOutputStream());
			sentence = inFromClient.readLine();
			System.out.println("Read from client: " + sentence);
			modified = sentence.toUpperCase() + '\n';
			outToClient.writeBytes(modified);
		}
	}
}
