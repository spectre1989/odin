using UnityEngine;
using System.Net.Sockets;

public class Client : MonoBehaviour 
{
	private Socket socket;

	private void OnEnable()
	{
		this.socket = new Socket( AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp );
	}

	private void OnDisable()
	{
		this.socket.Shutdown( SocketShutdown.Both );
	}
}
