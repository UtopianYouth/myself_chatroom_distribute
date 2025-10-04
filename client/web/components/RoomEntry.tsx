import NameAvatar from "@/components/NameAvatar";
import { useCallback } from "react";

const formatTimestamp = (timestamp: number): string => {
  if (!timestamp) return "";
  const date = new Date(timestamp);
  const now = new Date();
  const startOfToday = new Date(now.getFullYear(), now.getMonth(), now.getDate());
  const startOfYesterday = new Date(now.getFullYear(), now.getMonth(), now.getDate() - 1);

  if (date >= startOfToday) {
    return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', hour12: false });
  } else if (date >= startOfYesterday) {
    return "昨天";
  } else {
    return date.toLocaleDateString([], { month: 'numeric', day: 'numeric' });
  }
};

// A clickable chat room card
export default function RoomEntry({
  id,
  name,
  lastMessage,
  lastMessageTimestamp,
  selected,
  onClick,
  creatorId,
  currentUserId,
  unreadCount = 0,
}: {
  id: string;
  name: string;
  lastMessage: string;
  lastMessageTimestamp: number;
  selected: boolean;
  onClick: (roomId: string) => void;
  creatorId?: string;
  currentUserId?: string;
  unreadCount?: number;
}) {
  const isMyRoom = creatorId && currentUserId && creatorId === currentUserId;
  
  const onClickCallback = useCallback(() => onClick(id), [onClick, id]);

  return (
    <div
      className={`flex px-4 py-3 cursor-pointer transition-all duration-300 rounded-xl hover:bg-white/60 hover:shadow-sm ${
        selected ? 'bg-gradient-to-r from-[var(--primary-color)]/20 to-[var(--primary-light)]/20 border-l-4 border-[var(--primary-color)] shadow-sm' : ''
      }`}
      onClick={onClickCallback}
    >
      <div className="pr-3 flex flex-col justify-center">
        <NameAvatar name={name} />
      </div>
      <div className="flex-1 flex flex-col overflow-hidden min-w-0">
        <div className="flex items-center justify-between">
          <p className="m-0 font-semibold text-gray-800 truncate" data-testid="room-name">
            {name}
          </p>
          <p className="m-0 text-xs text-gray-500 whitespace-nowrap">
            {formatTimestamp(lastMessageTimestamp)}
          </p>
        </div>
        <div className="flex items-center justify-between mt-1">
          <p className="m-0 overflow-hidden text-ellipsis whitespace-nowrap text-gray-600 text-sm">
            {lastMessage}
          </p>
          {unreadCount > 0 && (
            <span className="bg-red-500 text-white text-xs font-bold rounded-full h-5 w-5 flex items-center justify-center">
              {unreadCount > 9 ? '9+' : unreadCount}
            </span>
          )}
        </div>
      </div>
    </div>
  );
}
