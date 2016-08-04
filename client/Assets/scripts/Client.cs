using UnityEngine;
using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;

public class Client : MonoBehaviour 
{
	public Transform playerObject;
	public GameObject playerObjectPrefab;

	private Socket socket;
	private byte[] buffer;
	private IPEndPoint serverEndPoint;
	private UInt16 slot;
	private List<Transform> playerObjectPool;
	private bool inDestruction;

	private enum ClientMessage : byte
	{
		Join,		// tell server we're new here
		Leave,		// tell server we're leaving
		Input 		// tell server our user input
	};

	private enum ServerMessage : byte
	{
		JoinResult, // tell client they're accepted/rejected
		State 		// tell client game state
	};

	private void OnEnable()
	{
		this.socket = new Socket( AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp );
		this.socket.Blocking = false;
		this.buffer = new byte[1024];
		this.serverEndPoint = new IPEndPoint( IPAddress.Loopback, 9999 );
		this.slot = 0xFFFF;
		this.playerObjectPool = new List<Transform>();
		this.inDestruction = false;

		this.buffer[0] = (byte)ClientMessage.Join;
		this.socket.SendTo( this.buffer, 1, SocketFlags.None, this.serverEndPoint );
	}

	private void OnDisable()
	{
		if( this.slot != 0xFFFF )
		{
			this.buffer[0] = (byte)ClientMessage.Leave;
        	Array.Copy( BitConverter.GetBytes( this.slot ), 0, this.buffer, 1, sizeof( UInt16 ) );
		}
 		
        this.socket.SendTo( this.buffer, this.serverEndPoint );
        this.socket.Shutdown( SocketShutdown.Both );
		this.socket.Close();
		this.socket = null;
		this.buffer = null;
		this.serverEndPoint = null;

		if( !this.inDestruction )
		{
			foreach( Transform t in this.playerObjectPool )
			{
				Destroy( t.gameObject );
			}
		}
		this.playerObjectPool = null;
	}

	private void OnApplicationQuit()
	{
		this.inDestruction = true;
	}

	private void Update()
	{
		if( this.slot == 0xFFFF )
		{
			while( this.socket.Available > 0 )
			{
				EndPoint remoteEP = new IPEndPoint( 0, 0 );
				this.socket.ReceiveFrom( this.buffer, ref remoteEP );

				if( this.buffer[0] == (byte)ServerMessage.JoinResult )
				{
					if( this.buffer[1] != 0 )
					{
						this.slot = BitConverter.ToUInt16( this.buffer, 2 );
						Debug.Log( "server assigned us slot " + this.slot.ToString() );
						break;
					}
					else
					{
						Debug.Log( "server didn't let us in" );
						break;
					}
				}
			}

			if( this.slot == 0xFFFF )
			{
				return;
			}
		}

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
		this.buffer[0] = (byte)ClientMessage.Input;
        Array.Copy( BitConverter.GetBytes(this.slot), 0, this.buffer, 1, sizeof( UInt16 ) );
		this.buffer[3] = input;
		this.socket.SendTo( this.buffer, 4, SocketFlags.None, this.serverEndPoint );

		// read state
		while( this.socket.Available > 0 )
		{
			EndPoint remoteEP = new IPEndPoint( 0, 0 );
			int packetSize = this.socket.ReceiveFrom( this.buffer, ref remoteEP );

			// unpack game state
			int readIndex = 1;
            int currentPlayerObject = 0;
			while( readIndex < packetSize )
			{
				UInt16 ownerSlot = BitConverter.ToUInt16( this.buffer, readIndex );
				readIndex += sizeof( UInt16 );

				Vector3 playerPosition = new Vector3( 0.0f, 0.0f, 0.0f );

				playerPosition.x = BitConverter.ToSingle( this.buffer, readIndex );
				readIndex += sizeof( float );

				playerPosition.z = BitConverter.ToSingle( this.buffer, readIndex );
				readIndex += sizeof( float );

				float playerFacing = BitConverter.ToSingle( this.buffer, readIndex );
                readIndex += sizeof( float );

				Transform t = this.playerObject;
				if( ownerSlot != this.slot )
				{
					if( currentPlayerObject < this.playerObjectPool.Count )
					{
						t = this.playerObjectPool[currentPlayerObject];
						t.gameObject.SetActive( true );
					}
					else
					{
						t = ( GameObject.Instantiate( playerObjectPrefab ) as GameObject ).transform;
						this.playerObjectPool.Add( t );
					}
					++currentPlayerObject;
				}

				t.position = playerPosition;
				t.rotation = Quaternion.Euler( 0.0f, playerFacing * Mathf.Rad2Deg, 0.0f );
			}

            for(; currentPlayerObject < this.playerObjectPool.Count; ++currentPlayerObject)
            {
                this.playerObjectPool[currentPlayerObject].gameObject.SetActive(false);
            }
        }
	}
}
