import RoomEntry from "@/components/RoomEntry";
import { Room } from "@/lib/apiTypes";

interface ChatRoomSectionProps {
  title: string;
  rooms: Room[];
  currentRoomId: string | null;
  onClickRoom: (roomId: string) => void;
  icon: string;
  currentUserId: string;
  isActive: boolean;
  unreadMessages: Record<string, number>;
}

export default function ChatRoomSection({
  title,
  rooms,
  currentRoomId,
  onClickRoom,
  icon,
  currentUserId,
  isActive,
  unreadMessages,
}: ChatRoomSectionProps) {
  if (rooms.length === 0 && !isActive) {
    return null;
  }

  return (
    <div className={`transition-all duration-300 ease-in-out ${isActive ? 'opacity-100' : 'hidden'}`}>
      <div className="space-y-1">
        {rooms.map((room) => (
          <RoomEntry
            key={room.id}
            id={room.id}
            name={room.name}
            lastMessage={room.messages[room.messages.length - 1]?.content || "暂无消息"}
            lastMessageTimestamp={room.messages[room.messages.length - 1]?.timestamp || 0}
            selected={room.id === currentRoomId}
            onClick={onClickRoom}
            creatorId={room.creator_id}
            currentUserId={currentUserId}
            unreadCount={unreadMessages[room.id] || 0}
          />
        ))}
      </div>
    </div>
  );
}