// A message in the chat

import NameAvatar from "@/components/NameAvatar";

function formatDate(date: number): string {
  return new Intl.DateTimeFormat("zh-CN", {
    month: "short",
    day: "numeric",
    hour: "2-digit",
    minute: "2-digit"
  }).format(new Date(date));
}

// A message in the conversation screen, sent by a user that's not ourselves
export const OtherUserMessage = ({
  username,
  content,
  timestamp,
}: {
  username: string;
  content: string;
  timestamp: number;
}) => {
  return (
    <div className="flex items-start justify-start space-x-3 px-3 py-1 hover:bg-white/10 transition-colors duration-200">
      <div className="flex-shrink-0">
        <NameAvatar name={username} />
      </div>
      <div className="flex-1 min-w-0 flex flex-col items-start">
        <div className="flex items-center space-x-2 mb-1">
          <span className="text-sm font-medium text-gray-800">{username}</span>
          <span className="text-sm text-gray-500">{formatDate(timestamp)}</span>
        </div>
        <div className="rounded-xl rounded-tl-sm px-4 py-3 shadow-sm max-w-[90%] bg-blue-200 bg-opacity-80">
          <p className="m-0 text-gray-800 leading-relaxed text-base" style={{ 
            overflowWrap: "break-word", 
            whiteSpace: "pre-wrap",
            wordBreak: "break-word",
            lineHeight: '1.5'
          }}>{content}</p>
        </div>
      </div>
    </div>
  );
};

// A message in the conversation screen, sent by ourselves
export const MyMessage = ({
  username,
  content,
  timestamp,
}: {
  username?: string;
  content: string;
  timestamp: number;
}) => {
  return (
    <div className="flex items-start justify-end space-x-3 px-3 py-1 hover:bg-white/10 transition-colors duration-200">
      <div className="flex-1 min-w-0 flex flex-col items-end">
        <div className="flex items-center space-x-2 mb-1">
          {username && <span className="text-sm font-medium text-gray-800">{username}</span>}
          <span className="text-sm text-gray-500">{formatDate(timestamp)}</span>
        </div>
        <div className="rounded-xl rounded-tr-sm px-4 py-3 shadow-sm max-w-[90%] bg-green-200 bg-opacity-80">
          <p className="m-0 text-gray-800 leading-relaxed text-base" style={{ 
            overflowWrap: "break-word", 
            whiteSpace: "pre-wrap",
            wordBreak: "break-word",
            lineHeight: '1.5'
          }}>{content}</p>
        </div>
      </div>
      <div className="flex-shrink-0">
        <NameAvatar name={username || ''} />
      </div>
    </div>
  );
};
