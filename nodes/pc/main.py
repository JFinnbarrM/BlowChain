

# Creates each thread as a task to run them sumultaneously.
async def main():
    # Create tasks for all required functions.
    # task_base_observer = asyncio.create_task(base_observer())
    # task_screen_connector = asyncio.create_task(viewer_connector())
    # task_tago_sender = asyncio.create_task(tago_sender())
    
    # Wait for both tasks (will run concurrently)
    # await asyncio.gather(task_base_observer, 
    #                      task_screen_connector, 
    #                      task_tago_sender)

# When the script is run, start the main function.
if __name__ == "__main__":
    # try:
    #     asyncio.run(main())
    # except Exception as e:
    #     print(f"Error: {e}")
    pass