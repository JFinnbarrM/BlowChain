
import asyncio

from lockbox_client import LockboxClient

# Creates each thread as a task to run them sumultaneously.
async def main():
    # Create tasks for all required functions.
    lockbox_client = LockboxClient()
    task_lockbox_client = asyncio.create_task(lockbox_client.run())
    
    # Wait for all tasks (will run concurrently)
    await asyncio.gather(
        task_lockbox_client
    )

# When the script is run, start the main function.
if __name__ == "__main__":
    try:
        asyncio.run(main())
    except Exception as e:
        print(f"Error: {e}")
