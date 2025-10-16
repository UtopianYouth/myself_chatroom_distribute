/* eslint-disable */
import Head from "@/components/Head";
import Header from "@/components/Header";
import RoomEntry from "@/components/RoomEntry";
import ChatRoomSection from "@/components/ChatRoomSection";
import { useCallback, useEffect, useReducer, useRef, useState } from "react";
import {
  parseWebsocketMessage,
  serializeMessagesEvent,
  serializeCreateRoomEvent,
} from "@/lib/apiSerialization";
import { ServerMessage, Room, User } from "@/lib/apiTypes";
import { MyMessage, OtherUserMessage } from "@/components/Message";
import MessageInputBar from "@/components/MessageInputBar";
import autoAnimate from "@formkit/auto-animate";
import { useRouter } from "next/router";
import { clearHasAuth } from "@/lib/hasAuth";
// The chat screen. This component uses React state management using
// useReducer.

// The type of the state that useReducer handles
type State = {
  currentUser: User | null;
  loading: boolean;
  rooms: Record<string, Room>; // id => room
  currentRoomId: string | null;
  unreadMessages: Record<string, number>; // roomId => unread count
  announcements: Announcement[];
  users: Record<string, User>;
};

// New type for announcement
type Announcement = {
  id: string;
  message: string;
};

// To be dispatched when the hello event is received. Sets the initial state
type SetInitialStateAction = {
  type: "set_initial_state";
  payload: {
    currentUser: User;
    rooms: Room[];
  };
};

// Adds a batch of messages to a certain room
type AddMessagesAction = {
  type: "add_messages";
  payload: {
    roomId: string;
    messages: ServerMessage[];
  };
};

// Sets the current active room (e.g. when the user clicks in a room)
type SetCurrentRoomAction = {
  type: "set_current_room";
  payload: {
    roomId: string;
  };
};

// Adds a new room
type AddNewRoomAction = {
  type: "add_new_room";
  payload: {
    roomId: string;
    roomName: string;
    creatorId: string;
  };
};

// 添加历史消息到房间
type UpdateRoomHistoryAction = {
  type: "update_room_history";
  payload: {
    roomId: string;
    messages: ServerMessage[];
    hasMoreMessages: boolean;
  };
};

type RemoveAnnouncementAction = {
  type: 'remove_announcement';
  payload: { id: string };
};

// Any of the above
type Action = SetInitialStateAction | AddMessagesAction | SetCurrentRoomAction | AddNewRoomAction | UpdateRoomHistoryAction | RemoveAnnouncementAction;

// A Message, either from our user or from another user
const Message = ({
  userId,
  username,
  content,
  timestamp,
  currentUserId,
}: {
  userId: string;
  username: string;
  content: string;
  timestamp: number;
  currentUserId: string;
}) => {
  if (userId === currentUserId) {
    return <MyMessage username={username} content={content} timestamp={timestamp} />;
  } else {
    return (
      <OtherUserMessage
        content={content}
        timestamp={timestamp}
        username={username}
      />
    );
  }
};

// Helpers to process actions
const addNewMessages = (
  messages: ServerMessage[],
  newMessages: ServerMessage[],
): ServerMessage[] => {
  const res = [...newMessages];
  res.sort((a, b) => a.timestamp - b.timestamp);
  return messages.concat(res);
};

function doSetInitialState(action: SetInitialStateAction): State {
  const { rooms, currentUser } = action.payload;
  const roomsById: Record<string, Room> = {};
  const usersById: Record<string, User> = {};

  if (currentUser) {
    usersById[currentUser.id] = currentUser;
  }

  for (const room of rooms) {
    room.messages.sort((a, b) => a.timestamp - b.timestamp);
    roomsById[room.id] = room;
    for (const message of room.messages) {
      if (!usersById[message.user.id]) {
        usersById[message.user.id] = message.user;
      }
    }
  }
  
  // 初始排序房间
  const sortedRooms = Object.values(roomsById);
  sortRoomsByLastMessageInplace(sortedRooms);
  
  return {
    currentUser,
    loading: false,
    rooms: roomsById,
    currentRoomId: findRoomWithLatestMessage(sortedRooms)?.id || null,
    unreadMessages: {}, // 初始化为空对象
    announcements: [],
    users: usersById,
  };
}

function doAddMessages(state: State, action: AddMessagesAction): State {
  // Find the room
  const room = state.rooms[action.payload.roomId];
  if (!room) return state; // We don't know what the server is talking about

  // Add the new messages to the array
  const updatedRooms = {
    ...state.rooms,
    [action.payload.roomId]: {
      ...room,
      messages: addNewMessages(room.messages, action.payload.messages),
    },
  };

  // 强制重新排序房间列表
  const sortedRooms = Object.values(updatedRooms);
  sortRoomsByLastMessageInplace(sortedRooms);

  // 更新未读消息计数
  const newMessages = action.payload.messages;
  let updatedUnreadMessages = { ...state.unreadMessages };
  
  // 如果当前不在这个房间，增加未读计数
  if (state.currentRoomId !== action.payload.roomId) {
    const currentUnread = updatedUnreadMessages[action.payload.roomId] || 0;
    updatedUnreadMessages[action.payload.roomId] = currentUnread + newMessages.length;
  }

  const newUsers = {...state.users};
  for (const message of action.payload.messages) {
    if (!newUsers[message.user.id]) {
      newUsers[message.user.id] = message.user;
    }
  }

  return {
    ...state,
    rooms: updatedRooms,
    unreadMessages: updatedUnreadMessages,
    users: newUsers,
  };
}

function getLastMessageTimestamp(room: Room): number {
  return room.messages.length > 0 ? room.messages[room.messages.length - 1].timestamp : 0;
}

function sortRoomsByLastMessageInplace(rooms: Room[]) {
  rooms.sort(
    (r1, r2) => getLastMessageTimestamp(r2) - getLastMessageTimestamp(r1),
  );
}

function findRoomWithLatestMessage(rooms: Room[]): Room | null {
  return rooms.reduce((prev, current) => {
    if (prev === null) return current;
    else
      return getLastMessageTimestamp(prev) > getLastMessageTimestamp(current)
        ? prev
        : current;
  }, null);
}

// The reducer
function reducer(state: State, action: Action): State {
  const { type, payload } = action;
  switch (type) {
    case "set_initial_state":
      return doSetInitialState(action);
    case "add_messages":    
      return doAddMessages(state, action);
    case "set_current_room":
      // 切换到新房间时，重置该房间的未读计数
      const updatedUnreadMessages = { ...state.unreadMessages };
      if (payload.roomId) {
        updatedUnreadMessages[payload.roomId] = 0;
      }
      return {
        ...state,
        currentRoomId: payload.roomId,
        unreadMessages: updatedUnreadMessages,
      };
    case "add_new_room":
      const creator = state.users[payload.creatorId];
      const creatorName = creator ? creator.username : payload.creatorId;
      const d = new Date();
      const time = `${d.getFullYear()}-${(d.getMonth() + 1).toString().padStart(2, '0')}-${d.getDate().toString().padStart(2, '0')} ${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}:${d.getSeconds().toString().padStart(2, '0')}`;
      const message = `${creatorName}在${time}创建了${payload.roomName}，快来该房间和他聊天吧！`;
      
      const newAnnouncement = {
        id: new Date().toISOString() + Math.random(),
        message: message,
      };

      const updatedAnnouncements = [...state.announcements, newAnnouncement];
      if (updatedAnnouncements.length > 5000) {
          updatedAnnouncements.shift();
      }

      return {
        ...state,
        rooms: {
          ...state.rooms,
          [payload.roomId]: {
            id: payload.roomId,
            name: payload.roomName,
            messages: [],
            hasMoreMessages: false,
            creator_id: payload.creatorId || state.currentUser?.id || ""
          }
        },
        announcements: updatedAnnouncements,
      };
    case "update_room_history":
      const room = state.rooms[payload.roomId];
      if (!room) return state;
      const historicalMessages = [...payload.messages];
      historicalMessages.sort((a, b) => a.timestamp - b.timestamp);
      return {
        ...state,
        rooms: {
          ...state.rooms,
          [payload.roomId]: {
            ...room,
            messages: [...room.messages, ...historicalMessages],
            hasMoreMessages: payload.hasMoreMessages
          }
        }
      };
    case "remove_announcement":
      return {
        ...state,
        announcements: state.announcements.filter(ann => ann.id !== payload.id),
      };
    default:
      return state;
  }
}

// Retrieves the websocket URL to connect to. This is either embedded by
// the client generation process, defaulting to the host and port we're been
// served from
function getWebsocketURL(): string {

  const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
  const port = location.port;
  
  // 如果是标准端口(http:80/https:443)，不需要在URL中指定端口
  if (port === '80' || port === '443' || port === '') {
    return `${protocol}//${location.hostname}/api/ws`;
  }
  
  // 其他情况使用8080端口
  return `${protocol}//${location.hostname}:8080/api/ws`;
}

const AnnouncementBanner = ({ announcements, onRemove }: { announcements: Announcement[], onRemove: (id: string) => void }) => {
  const [currentAnnouncement, setCurrentAnnouncement] = useState<Announcement | null>(null);
  const [visible, setVisible] = useState(false);

  useEffect(() => {
    if (announcements.length > 0 && !currentAnnouncement) {
      setCurrentAnnouncement(announcements[0]);
    }
  }, [announcements, currentAnnouncement]);

  useEffect(() => {
    if (currentAnnouncement) {
      setVisible(true);
      const timer = setTimeout(() => {
        setVisible(false);
        // After fade out animation, remove from queue
        const removeTimer = setTimeout(() => {
          onRemove(currentAnnouncement.id);
          setCurrentAnnouncement(null);
        }, 500); // matches transition duration
        return () => clearTimeout(removeTimer);
      }, 5000);
      return () => {
        clearTimeout(timer);
      }
    }
  }, [currentAnnouncement, onRemove]);

  if (!currentAnnouncement) {
    return null;
  }

  return (
    <div
      className={`absolute top-4 left-1/2 -translate-x-1/2 w-11/12 max-w-lg p-2 z-50 transition-all duration-500 ease-in-out text-center ${
        visible ? 'opacity-100 translate-y-0' : 'opacity-0 -translate-y-full'
      }`}
    >
      <p className="font-bold text-xl text-blue-400 drop-shadow-lg">{currentAnnouncement.message}</p>
    </div>
  );
};

// Exposed for the sake of testing
export const ChatScreen = ({
  rooms,
  currentRoom,
  onClickRoom,
  currentUserId,
  onMessage,
  onCreateRoom,
  onRequestHistory,
  showCreateRoomDialog,
  setShowCreateRoomDialog,
  unreadMessages,
  announcements,
  onRemoveAnnouncement,
  creationError,
  setCreationError,
}: {
  rooms: Room[];
  currentRoom: Room | null;
  currentUserId: string;
  onClickRoom: (roomId: string) => void;
  onMessage: (msg: string) => void;
  onCreateRoom: (roomName: string) => void;
  onRequestHistory: (roomId: string, firstMessageId: string) => void;
  showCreateRoomDialog: boolean;
  setShowCreateRoomDialog: (show: boolean) => void;
  unreadMessages: Record<string, number>;
  announcements: Announcement[];
  onRemoveAnnouncement: (id: string) => void;
  creationError: string | null;
  setCreationError: (error: string | null) => void;
}) => {
  // rooms prop is already sorted by the parent component.
  // We filter them into "my rooms" and "other rooms". The filter operation preserves the sort order.
  const myRooms = rooms.filter(room => room.creator_id === currentUserId);
  const otherRooms = rooms.filter(room => room.creator_id !== currentUserId);
  
  const [activeTab, setActiveTab] = useState<'all' | 'my' | 'other'>('all');
  // Animate transitions. We use this instead of useAutoAnimate because the
  // latter doesn't play well with Jest
  const messagesRef = useCallback((elm) => {
    if (elm) autoAnimate(elm);
  }, []);
  const roomsRef = useCallback((elm) => {
    if (elm) autoAnimate(elm);
  }, []);
  
  const currentRoomMessages = currentRoom?.messages || [];
  const [roomName, setRoomName] = useState("");
  const [isLoading, setIsLoading] = useState(false);
  const [isAtTop, setIsAtTop] = useState(false);
  
  // 添加对消息容器的引用
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const messagesContainerRef = useRef<HTMLDivElement>(null);
  
  useEffect(() => {
    if (!isLoading) {
      messagesEndRef.current?.scrollIntoView();
    }
  }, [currentRoom?.id, currentRoomMessages, isLoading]);

  // 记录加载前的滚动位置
  const [previousScrollHeight, setPreviousScrollHeight] = useState<number>(0);

  // 处理历史消息加载后的滚动
  useEffect(() => {
    if (isLoading && messagesContainerRef.current) {
      const container = messagesContainerRef.current;
      const newScrollHeight = container.scrollHeight;
      const scrollDiff = newScrollHeight - previousScrollHeight;
      if (scrollDiff > 0) {
        container.scrollTop = scrollDiff;
      }
      setIsLoading(false);
    }
  }, [currentRoomMessages, isLoading, previousScrollHeight]);

  // 修改滚动处理函数
  const handleScroll = useCallback((e: React.UIEvent<HTMLDivElement>) => {
    const wheelEvent = e.nativeEvent as WheelEvent;
    const container = messagesContainerRef.current;
    console.log("handleScroll deltaY: ", wheelEvent.deltaY, "isAtTop:", isAtTop, "hasMoreMessages:", currentRoom?.hasMoreMessages);
    
    if (wheelEvent.deltaY < 0) {
      if (isAtTop && currentRoom?.hasMoreMessages && !isLoading) {
        setIsLoading(true);
        // 记录当前滚动高度
        if (container) {
          setPreviousScrollHeight(container.scrollHeight);
        }
        const firstMessage = currentRoomMessages[currentRoomMessages.length - 1];
        if (firstMessage) {
          onRequestHistory(currentRoom.id, firstMessage.id);
          setIsAtTop(false);
        }
      } else {
        setIsAtTop(true);
      }
    } else {
      setIsAtTop(false);
    }
  }, [currentRoom, currentRoomMessages, onRequestHistory, isAtTop, isLoading]);

  const handleCreateRoom = () => {
    if (roomName.trim()) {
      onCreateRoom(roomName.trim());
    }
  };

  // 绑定 ESC 关闭创建房间弹窗
  useEffect(() => {
    if (!showCreateRoomDialog) return;
    const onKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        setShowCreateRoomDialog(false);
        setCreationError(null);
        setRoomName("");
      }
    };
    window.addEventListener('keydown', onKeyDown);
    return () => {
      window.removeEventListener('keydown', onKeyDown);
    };
  }, [showCreateRoomDialog, setShowCreateRoomDialog, setCreationError]);

  // 弹窗打开时重置输入与错误提示
  useEffect(() => {
    if (showCreateRoomDialog) {
      setRoomName("");
      setCreationError(null);
    }
  }, [showCreateRoomDialog, setCreationError]);

  return (
    <div className="h-full flex bg-white">
      {/* 聊天室组件 */}
      <div className="flex h-full w-full">
        
        {/* 左侧房间管理区域 */}
        <div className="w-80 flex flex-col bg-gray-50 border-r border-gray-200">
          {/* 房间分类按钮*/}
          <div className="p-4">
            <div className="flex space-x-2 w-full">
              <button
                onClick={() => setActiveTab('all')}
                className={`flex-1 flex items-center justify-center p-3 rounded-xl transition-colors duration-200 border-none ${
                  activeTab === 'all'
                    ? 'bg-blue-100 text-blue-700 font-semibold'
                    : 'text-gray-500 hover:bg-gray-200/50 hover:text-gray-800'
                }`}
              >
                <svg className="w-4 h-4 mr-2" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1} d="M8 12h.01M12 12h.01M16 12h.01M21 12c0 4.418-4.03 8-9 8a9.863 9.863 0 01-4.255-.949L3 20l1.395-3.72C3.512 15.042 3 13.574 3 12c0-4.418 4.03-8 9-8s9 3.582 9 8z" />
                </svg>
                <span className={`ml-2 text-xs px-2 py-0.5 rounded-full font-medium ${
                  activeTab === 'all' ? 'bg-white/40 text-white' : 'bg-gray-300/50 text-gray-600'
                }`}>
                  {Object.values(unreadMessages).reduce((total, count) => total + count, 0)}
                </span>
              </button>
              <button
                onClick={() => setActiveTab('my')}
                className={`flex-1 flex items-center justify-center p-3 rounded-xl transition-colors duration-200 border-none ${
                  activeTab === 'my'
                    ? 'bg-blue-100 text-blue-700 font-semibold'
                    : 'text-gray-500 hover:bg-gray-200/50 hover:text-gray-800'
                }`}
                title="我的房间"
              >
                <svg className="w-4 h-4" fill="currentColor" viewBox="0 0 20 20">
                  <path fillRule="evenodd" d="M10 9a3 3 0 100-6 3 3 0 000 6zm-7 9a7 7 0 1114 0H3z" clipRule="evenodd" />
                </svg>
              </button>
              <button
                onClick={() => setActiveTab('other')}
                className={`flex-1 flex items-center justify-center p-3 rounded-xl transition-colors duration-200 border-none ${
                  activeTab === 'other'
                    ? 'bg-blue-100 text-blue-700 font-semibold'
                    : 'text-gray-500 hover:bg-gray-200/50 hover:text-gray-800'
                }`}
                title="其它房间"
              >
                <svg className="w-4 h-4" fill="currentColor" viewBox="0 0 20 20">
                  <path d="M13 6a3 3 0 11-6 0 3 3 0 016 0zM18 8a2 2 0 11-4 0 2 2 0 014 0zM14 15a4 4 0 00-8 0v3h8v-3zM6 8a2 2 0 11-4 0 2 2 0 014 0zM16 18v-3a5.972 5.972 0 00-.75-2.906A3.005 3.005 0 0119 15v3h-3zM4.75 12.094A5.973 5.973 0 004 15v3H1v-3a3 3 0 013.75-2.906z" />
                </svg>
              </button>
            </div>
          </div>

          {/* 房间列表 */}
          <div className="flex-1 overflow-y-auto" ref={roomsRef}>
            {/* 所有聊天室 */}
            <ChatRoomSection
              title=""
              rooms={rooms}
              currentRoomId={currentRoom?.id || null}
              onClickRoom={onClickRoom}
              icon="💬"
              currentUserId={currentUserId}
              isActive={activeTab === 'all'}
              unreadMessages={unreadMessages}
            />

            {/* 我创建的聊天室 */}
            <ChatRoomSection
              title=""
              rooms={myRooms}
              currentRoomId={currentRoom?.id || null}
              onClickRoom={onClickRoom}
              icon="👑"
              currentUserId={currentUserId}
              isActive={activeTab === 'my'}
              unreadMessages={unreadMessages}
            />

            {/* 他人创建的聊天室 */}
            <ChatRoomSection
              title=""
              rooms={otherRooms}
              currentRoomId={currentRoom?.id || null}
              onClickRoom={onClickRoom}
              icon="💬"
              currentUserId={currentUserId}
              isActive={activeTab === 'other'}
              unreadMessages={unreadMessages}
            />
          </div>
        </div>

        {/* 右侧聊天区域 */}
        <div className="flex-1 flex flex-col bg-white relative">
          <AnnouncementBanner announcements={announcements} onRemove={onRemoveAnnouncement} />
          {currentRoom ? (
            <div className="flex flex-col h-full">
              {/* 聊天标题栏 */}
              <div className="flex-shrink-0 p-4 border-b border-gray-200" style={{
                padding: '12px 16px'
              }}>
                <div className="flex items-center justify-between">
                  <div className="flex items-center">
                    <div 
                      className="w-10 h-10 rounded-xl flex items-center justify-center text-white font-bold mr-3 shadow-lg bg-blue-500"
                      style={{
                        boxShadow: '0 4px 15px rgba(59, 130, 246, 0.3)'
                      }}
                    >
                      <span className="text-base">{currentRoom.name.charAt(0).toUpperCase()}</span>
                    </div>
                    <div>
                      <h3 className="font-bold text-gray-800 text-base">{currentRoom.name}</h3>
                    </div>
                  </div>
                  <div className="flex space-x-1">
                    <button className="p-2 text-gray-600 hover:text-blue-500 transition-all duration-300 bg-transparent border-none focus:outline-none">
                      <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z" />
                      </svg>
                    </button>
                    <button className="p-2 text-gray-600 hover:text-blue-500 transition-all duration-300 bg-transparent border-none focus:outline-none">
                      <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 12h.01M12 12h.01M19 12h.01M6 12a1 1 0 11-2 0 1 1 0 012 0zm7 0a1 1 0 11-2 0 1 1 0 012 0zm7 0a1 1 0 11-2 0 1 1 0 012 0z" />
                      </svg>
                    </button>
                  </div>
                </div>
              </div>

              {/* 消息内容区域 */}
              <div
                className="flex-1 overflow-y-auto"
                style={{ 
                  scrollBehavior: "smooth",
                  minHeight: 0
                }}
                ref={messagesContainerRef}
                onWheel={handleScroll}
              >
                <div className="flex flex-col-reverse p-4" style={{ minHeight: '100%' }}>
                  <div ref={messagesEndRef} />
                  
                  {/* 消息列表 */}
                  {currentRoomMessages.length === 0 ? (
                    <div className="flex-1 flex items-center justify-center text-gray-500" style={{ minHeight: '200px' }}>
                      <div className="text-center">
                        <svg className="w-12 h-12 mx-auto mb-3 text-gray-300" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1} d="M8 12h.01M12 12h.01M16 12h.01M21 12c0 4.418-4.03 8-9 8a9.863 9.863 0 01-4.255-.949L3 20l1.395-3.72C3.512 15.042 3 13.574 3 12c0-4.418 4.03-8 9-8s9 3.582 9 8z" />
                        </svg>
                        <p className="text-gray-400 text-sm">暂无消息，开始对话吧</p>
                      </div>
                    </div>
                  ) : (
                    <div className="space-y-3">
                      {currentRoomMessages.map((msg) => (
                        <Message
                          key={msg.id}
                          userId={msg.user.id}
                          username={msg.user.username}
                          content={msg.content}
                          timestamp={msg.timestamp}
                          currentUserId={currentUserId}
                        />
                      ))}
                    </div>
                  )}
                  
                  {/* 历史记录提示 */}
                  {currentRoom && (
                    <div 
                      className={`text-center py-4 text-gray-500 text-xs transition-all duration-300 ease-in-out ${
                        isLoading ? 'opacity-50' : 'opacity-100'
                      }`}
                    >
                      {isLoading ? (
                        <div className="flex items-center justify-center gap-2">
                          <svg className="animate-spin h-4 w-4 text-blue-500" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
                            <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                            <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
                          </svg>
                          <span className="text-gray-600">加载中...</span>
                        </div>
                      ) : currentRoom.hasMoreMessages ? (
                        <div className="animate-pulse">
                          <p className="text-gray-500 text-xs">👆 向上滑动加载更多</p>
                        </div>
                      ) : (
                        <p className="text-gray-400 text-xs">已到达最早消息</p>
                      )}
                    </div>
                  )}
                </div>
              </div>

              {/*输入框区域*/}
              <div className="flex-shrink-0">
                <MessageInputBar onMessage={onMessage} />
              </div>
            </div>
          ) : (
            <div className="flex-1 flex items-center justify-center">
              <div className="text-center">
                <div 
                  className="w-20 h-20 rounded-2xl flex items-center justify-center mx-auto mb-4 shadow-lg bg-blue-500"
                  style={{
                    boxShadow: '0 8px 25px rgba(59, 130, 246, 0.3)'
                  }}
                >
                  <svg className="w-10 h-10 text-white" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8 12h.01M12 12h.01M16 12h.01M21 12c0 4.418-4.03 8-9 8a9.863 9.863 0 01-4.255-.949L3 20l1.395-3.72C3.512 15.042 3 13.574 3 12c0-4.418 4.03-8 9-8s9 3.582 9 8z" />
                  </svg>
                </div>
                <h3 className="text-xl font-bold text-gray-800 mb-2">欢迎来到聊天室</h3>
                <p className="text-gray-600 text-sm">选择一个聊天室开始对话</p>
              </div>
            </div>
          )}
        </div>
        
        {showCreateRoomDialog && (
          <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
            <div className="bg-white p-6 rounded-2xl min-w-[320px] flex flex-col shadow-2xl relative">
              <button
                className="absolute top-3 right-3 p-2 text-gray-400 hover:text-gray-600 transition-colors bg-transparent border-none focus:outline-none"
                aria-label="关闭"
                onClick={() => {
                  setShowCreateRoomDialog(false);
                  setCreationError(null);
                  setRoomName("");
                }}
              >
                <svg className="w-5 h-5" viewBox="0 0 24 24" fill="none" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M6 6l12 12M6 18L18 6" />
                </svg>
              </button>
              <h3 className="text-xl font-semibold mb-4 text-center text-gray-800">创建新聊天室</h3>
              <div className="flex items-center justify-center mb-2">
                <input
                  type="text"
                  value={roomName}
                  onChange={(e) => setRoomName(e.target.value)}
                  className={`border p-3 w-full rounded-xl focus:outline-none ${creationError ? 'border-red-500 focus:ring-2 focus:ring-red-500' : 'border-gray-300 focus:ring-2 focus:ring-blue-500 focus:border-transparent'}`}
                  aria-invalid={!!creationError}
                  placeholder="输入聊天室名称"
                  autoFocus
                />
              </div>
              {creationError && (
                <p className="text-red-500 text-sm mt-2 text-left" aria-live="polite" style={{ marginLeft: '5px' }}>
                  {creationError}
                </p>
              )}
              <div className="flex justify-end gap-3 mt-4">

                <button
                  className="px-5 py-2.5 bg-blue-500 text-white rounded-xl hover:scale-105 active:scale-95 transition-all duration-300 shadow-lg border-none focus:outline-none"
                  onClick={handleCreateRoom}
                >
                  确定
                </button>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};


// Websocket close code to signal that authentication is required
const CODE_POLICY_VIOLATION = 1008;

// The actual component
export default function ChatPage() {
  // State management
  const [showCreateRoomDialog, setShowCreateRoomDialog] = useState(false);
  const [creationError, setCreationError] = useState<string | null>(null);
  const [state, dispatch] = useReducer(reducer, {
    currentUser: null,
    loading: true,
    rooms: {},
    currentRoomId: null,
    unreadMessages: {},
    announcements: [],
    users: {},
  });

  // Send a websocket message to the server when the user hits enter
  const onMessageTyped = useCallback(
    (msg: string) => {
      if (!websocketRef.current) return;
      const evt = serializeMessagesEvent(state.currentRoomId, {
        content: msg,
      });
      websocketRef.current.send(evt);
    },
    [state.currentRoomId],
  );

  // Change the current active room when the user clicks on it
  const onClickRoom = useCallback(
    (roomId: string) => {
      dispatch({ type: "set_current_room", payload: { roomId } });
    },
    [dispatch],
  );

  const onCreateRoom = useCallback((roomName: string) => {
    if (!websocketRef.current) return;
    const evt = serializeCreateRoomEvent(roomName);
    websocketRef.current.send(evt);
  }, []);

  const openCreateRoomDialog = useCallback(() => {
    setShowCreateRoomDialog(true);
  }, []);

  const router = useRouter();

  // Handle server websocket messages
  type WebSocketMessageType = "hello" | "serverMessages" | "serverCreateRoom" | "serverRoomHistory";

  // 添加历史记录请求处理函数
  const onRequestHistory = useCallback((roomId: string, firstMessageId: string) => {
    if (!websocketRef.current) return;
    const request = {
      type: "requestRoomHistory",
      payload: {
        roomId: roomId,
        firstMessageId: firstMessageId,
        count: 30
      }
    };
    websocketRef.current.send(JSON.stringify(request));
  }, []);

  // 修改websocket消息处理函数，添加历史记录响应处理
  const onWebsocketMessage = useCallback((event: MessageEvent) => {
    const { type, payload } = parseWebsocketMessage(event.data);
    // console.log("onWebsocketMessage: ", event.data);
    switch (type) {
      case "hello":
        dispatch({
          type: "set_initial_state",
          payload: {
            currentUser: payload.me,
            rooms: payload.rooms,
          },
        });
        break;
      case "serverMessages":
        dispatch({
          type: "add_messages",
          payload: {
            roomId: payload.roomId,
            messages: payload.messages,
          },
        });
        break;
      case "serverCreateRoom":
        if (payload.roomId) {
          dispatch({
            type: "add_new_room",
            payload: {
              roomId: payload.roomId,
              roomName: payload.roomName,
              creatorId: payload.creatorId,
            },
          });
          setShowCreateRoomDialog(false);
          setCreationError(null);
        } else {
          setCreationError("该房间名已经被使用，请重新想一个吧~");
        }
        break;
      case "serverRoomHistory":
        dispatch({
          type: "update_room_history",
          payload: {
            roomId: payload.roomId,
            messages: payload.messages,
            hasMoreMessages: payload.hasMoreMessages
          }
        });
        break;
    }
  }, [dispatch, setCreationError, setShowCreateRoomDialog]);

  const onClose = useCallback(
    (event: CloseEvent) => {
      
      // On authentication failure, the server closes the websocket with this code.
      if (event.code === CODE_POLICY_VIOLATION) {
        clearHasAuth();
        router.replace("/login");
      } else {
        clearHasAuth();
        router.replace("/login");
        console.log("server error -> event.code:", event.code);
      }
      
    },
    [router],
  );

  const onRemoveAnnouncement = useCallback((id: string) => {
    dispatch({ type: 'remove_announcement', payload: { id } });
  }, [dispatch]);

  // Create a websocket
  const websocketRef = useRef<WebSocket | null>(null);
  useEffect(() => {
    const ws = new WebSocket(getWebsocketURL());
    ws.addEventListener("message", onWebsocketMessage);
    ws.addEventListener("close", onClose);
    websocketRef.current = ws;

    // 添加beforeunload事件监听器
    const handleBeforeUnload = () => {
      console.log("Page is being refreshed, closing websocket");
      if (websocketRef.current) {
        websocketRef.current.close();
      }
    };
    window.addEventListener('beforeunload', handleBeforeUnload);

    // 清理函数
    return () => {
      window.removeEventListener('beforeunload', handleBeforeUnload);
      if (websocketRef.current) {
        websocketRef.current.removeEventListener("message", onWebsocketMessage);
        websocketRef.current.removeEventListener("close", onClose);
        websocketRef.current.close();
      }
    };
  }, [onWebsocketMessage, onClose]);

  // Handle loading state
  if (state.loading) return <p>Loading...</p>;

  // The actual screen
  const currentRoom = state.rooms[state.currentRoomId] || null;
  const rooms = Object.values(state.rooms) as Room[];
  sortRoomsByLastMessageInplace(rooms);

  return (
    <>
      <Head />
      <div className="h-screen flex flex-col">
        <div className="flex-shrink-0">
          <Header 
            pageType="chat"
            onCreateRoom={openCreateRoomDialog} 
            currentUserId={state.currentUser?.id}
            username={state.currentUser?.username}
          />
        </div>
        <div className="flex-1 min-h-0">
          <ChatScreen
            rooms={rooms}
            currentRoom={currentRoom}
            currentUserId={state.currentUser.id}
            onClickRoom={onClickRoom}
            onMessage={onMessageTyped}
            onCreateRoom={onCreateRoom}
            onRequestHistory={onRequestHistory}
            showCreateRoomDialog={showCreateRoomDialog}
            setShowCreateRoomDialog={setShowCreateRoomDialog}
            unreadMessages={state.unreadMessages}
            announcements={state.announcements}
            onRemoveAnnouncement={onRemoveAnnouncement}
            creationError={creationError}
            setCreationError={setCreationError}
          />
        </div>
      </div>
    </>
  );
}
