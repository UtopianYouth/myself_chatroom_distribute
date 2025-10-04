import { useRef } from "react";

// The input bar where the user types messages
export default function MessageInputBar({
  onMessage,
}: {
  onMessage: (msg: string) => void;
}) {
  const inputRef = useRef<HTMLInputElement>(null);

  const handleSendMessage = () => {
    const messageContent = inputRef.current?.value;
    if (messageContent && messageContent.trim()) {
      onMessage(messageContent.trim());
      if (inputRef.current) {
        inputRef.current.value = "";
      }
    }
  };

  const onKeyDown = (event: React.KeyboardEvent<HTMLInputElement>) => {
    if (event.key === 'Enter') {
      event.preventDefault();
      handleSendMessage();
    }
  };

  return (
    <div className="p-4">
      <div className="flex items-center bg-gray-50 rounded-2xl p-2 border border-gray-200">
        <input
          ref={inputRef}
          type="text"
          placeholder="输入消息..."
          className="flex-1 border-none outline-none bg-transparent text-base focus:ring-0 h-10 px-2"
          onKeyDown={onKeyDown}
        />
        <button
          onClick={handleSendMessage}
          className="ml-2 p-2 bg-gray-200/80 text-gray-700 rounded-xl hover:bg-gray-300 transition-all duration-300 focus:outline-none flex-shrink-0 border-none"
        >
          <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M12 19V5m-7 7l7-7 7 7" />
          </svg>
        </button>
      </div>
    </div>
  );
}