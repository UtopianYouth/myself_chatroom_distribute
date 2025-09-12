import { useCallback, useEffect, useRef } from "react";

// The input bar where the user types messages
export default function MessageInputBar({
  onMessage,
}: {
  onMessage: (msg: string) => void;
}) {
  const inputRef = useRef<HTMLInputElement>(null);

  const onKeyDown = useCallback(
    (event: KeyboardEvent) => {
      // Focus the input when the user presses any key, to make interaction smoother
      // inputRef.current.focus();
      // Darren修改，只有当inputRef.current是当前活动元素时，才执行focus()方法
      if (document.activeElement == inputRef.current) {
        inputRef.current.focus();
      }

      // Invoke the passed callback when Entr is hit, then clear the message
      // if (event.key === "Enter") {
        if (event.key === "Enter" && document.activeElement === inputRef.current) {
        const messageContent = inputRef.current?.value;
        if (messageContent) {
          onMessage(messageContent);
          inputRef.current.value = "";
        }
      }
    },
    [onMessage],
  );

  useEffect(() => {
    window.addEventListener("keydown", onKeyDown);
    return () => {
      window.removeEventListener("keydown", onKeyDown);
    };
  }, [onKeyDown]);

  return (
    <div className="flex">
      <div
        className="flex-1 flex p-2"
        style={{ backgroundColor: "var(--boost-light-grey)" }}
      >
        <input
          type="text"
          className="flex-1 text-xl pl-4 pr-4 pt-2 pb-2 border-0 rounded-xl"
          placeholder="输入一条消息..."
          ref={inputRef}
        />
      </div>
    </div>
  );
}
