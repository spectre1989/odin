using UnityEngine;
using System;
using System.Net;
using System.Net.Sockets;

public class Client : MonoBehaviour 
{
	public Transform playerObject;

	private Socket socket;
	private byte[] buffer;
	private IPEndPoint serverEndPoint;

	private void OnEnable()
	{
		this.socket = new Socket( AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp );
		this.buffer = new byte[sizeof(float) * 3];
		this.serverEndPoint = new IPEndPoint( IPAddress.Loopback, 9999 );
	}

	private void OnDisable()
	{
		this.socket.Shutdown( SocketShutdown.Both );
		this.buffer = null;
		this.serverEndPoint = null;
	}

	private void Update()
	{
		// get input
		byte input = 0;

		if( Input.GetKey( KeyCode.W ) )
		{
			input |= 0x1;
		}
		if( Input.GetKey( KeyCode.S ) )
		{
			input |= 0x2;
		}
		if( Input.GetKey( KeyCode.A ) )
		{
			input |= 0x4;
		}
		if( Input.GetKey( KeyCode.D ) )
		{
			input |= 0x8;
		}

		// send to server
		this.buffer[0] = input;
		this.socket.SendTo( this.buffer, 1, SocketFlags.None, this.serverEndPoint );

		// wait for state packet
		EndPoint remoteEP = new IPEndPoint( 0, 0 );
		this.socket.ReceiveFrom( this.buffer, ref remoteEP );

		// unpack game state
		Vector3 playerPosition = new Vector3( 0.0f, 0.0f, 0.0f );

		int readIndex = 0;
		playerPosition.x = BitConverter.ToSingle( this.buffer, readIndex );
		readIndex += sizeof( float );

		playerPosition.z = BitConverter.ToSingle( this.buffer, readIndex );
		readIndex += sizeof( float );

		float playerFacing = BitConverter.ToSingle( this.buffer, readIndex );

		this.playerObject.position = playerPosition;
		this.playerObject.rotation = Quaternion.Euler( 0.0f, playerFacing * Mathf.Rad2Deg, 0.0f );
	}
}
