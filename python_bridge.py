import asyncio
import json
import pickle
import struct
from R4_public import R4_DETECTION
from Object_over_IP import DataClient

async def process_data(sim_ip, sim_port, cpp_ip, cpp_port):
    """
    Connect to the radar simulation server, deserialize data, and send it to the C++ client.
    """
    try:
        # Create a TCP client to connect to the simulation server
        print(f"Connecting to simulation server at {sim_ip}:{sim_port}...")
        reader, writer = await asyncio.open_connection(sim_ip, sim_port)
        print("Connected to simulation server.")

        # Create a TCP server for the C++ program
        print(f"Starting C++ server at {cpp_ip}:{cpp_port}...")
        cpp_server = await asyncio.start_server(handle_cpp_client, cpp_ip, cpp_port)
        async with cpp_server:
            print("C++ server started. Waiting for connections...")
            await cpp_server.serve_forever()
    except Exception as e:
        print(f"Error in process_data: {e}")


async def handle_cpp_client(reader, writer):
    """
    Handle incoming connections from the C++ program.
    """
    try:
        while True:
            # Receive radar data from the simulation server
            frame_size_data = await reader.read(4)
            if not frame_size_data:
                print("Connection closed by simulation server.")
                break

            # Unpack frame size
            frame_size = struct.unpack('>I', frame_size_data)[0]

            # Receive frame data
            frame_data = await reader.read(frame_size)
            radar_frame = pickle.loads(frame_data)

            # Convert radar frame to JSON
            json_frame = json.dumps(radar_frame, default=lambda o: o._asdict() if isinstance(o, R4_DETECTION) else o)

            # Send JSON data to the C++ program
            writer.write(json_frame.encode('utf-8'))
            await writer.drain()

    except Exception as e:
        print(f"Error handling C++ client: {e}")
    finally:
        writer.close()
        await writer.wait_closed()


if __name__ == "__main__":
    SIM_IP = "192.168.10.10"  # IP of Simulation.py
    SIM_PORT = 4035           # Port of Simulation.py
    CPP_IP = "127.0.0.1"      # Localhost for the C++ program
    CPP_PORT = 5000           # Port for the C++ program

    asyncio.run(process_data(SIM_IP, SIM_PORT, CPP_IP, CPP_PORT))