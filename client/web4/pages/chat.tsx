/* eslint-disable */
import Head from "@/components/Head";
import Header from "@/components/Header";
import RoomEntry from "@/components/RoomEntry";
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

// Any of the above
type Action = SetInitialStateAction | AddMessagesAction | SetCurrentRoomAction | AddNewRoomAction | UpdateRoomHistoryAction;

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
    return <MyMessage  content={content} timestamp={timestamp} />;
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
  res.reverse();
  return res.concat(messages);
};

function doSetInitialState(action: SetInitialStateAction): State {
  const { rooms, currentUser } = action.payload;
  const roomsById = {};
  for (const room of rooms) roomsById[room.id] = room;
  return {
    currentUser,
    loading: false,
    rooms: roomsById,
    currentRoomId: findRoomWithLatestMessage(rooms)?.id || null,
  };
}

function doAddMessages(state: State, action: AddMessagesAction): State {
  // Find the room
  const room = state.rooms[action.payload.roomId];
  if (!room) return state; // We don't know what the server is talking about

  // Add the new messages to the array
  return {
    ...state,
    rooms: {
      ...state.rooms,
      [action.payload.roomId]: {
        ...room,
        messages: addNewMessages(room.messages, action.payload.messages),
      },
    },
  };
}

function getLastMessageTimestamp(room: Room): number {
  return room.messages.length > 0 ? room.messages[0].timestamp : 0;
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
      return {
        ...state,
        currentRoomId: payload.roomId,
      };
    case "add_new_room":
      return {
        ...state,
        rooms: {
          ...state.rooms,
          [payload.roomId]: {
            id: payload.roomId,
            name: payload.roomName,
            messages: [],
            hasMoreMessages: false
          }
        }
      };
    case "update_room_history":
      const room = state.rooms[payload.roomId];
      if (!room) return state;
      return {
        ...state,
        rooms: {
          ...state.rooms,
          [payload.roomId]: {
            ...room,
            messages: [...room.messages, ...payload.messages],
            hasMoreMessages: payload.hasMoreMessages
          }
        }
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

// Exposed for the sake of testing
export const ChatScreen = ({
  rooms,
  currentRoom,
  onClickRoom,
  currentUserId,
  onMessage,
  onCreateRoom,
  onRequestHistory,
}: {
  rooms: Room[];
  currentRoom: Room | null;
  currentUserId: string;
  onClickRoom: (roomId: string) => void;
  onMessage: (msg: string) => void;
  onCreateRoom: (roomName: string) => void;
  onRequestHistory: (roomId: string, firstMessageId: string) => void;
}) => {
  // Animate transitions. We use this instead of useAutoAnimate because the
  // latter doesn't play well with Jest
  const messagesRef = useCallback((elm) => {
    if (elm) autoAnimate(elm);
  }, []);
  const roomsRef = useCallback((elm) => {
    if (elm) autoAnimate(elm);
  }, []);
  
  const currentRoomMessages = currentRoom?.messages || [];
  const [showDialog, setShowDialog] = useState(false);
  const [roomName, setRoomName] = useState("");
  const [isLoading, setIsLoading] = useState(false);
  const [isAtTop, setIsAtTop] = useState(false);
  
  // 添加对消息容器的引用
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const messagesContainerRef = useRef<HTMLDivElement>(null);
  
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
    console.log("handleCreateRoom");
    if (roomName.trim()) {
      console.log("handleCreateRoom: ", roomName.trim());
      onCreateRoom(roomName.trim());
      setRoomName("");
      setShowDialog(false);
    }
  };

  return (
    <div
      className="flex-1 flex min-h-0"
      style={{ borderTop: "1px solid var(--boost-light-grey)" }}
    >
      <div className="flex-1 flex flex-col">
        <button
          className="m-2 p-2 bg-blue-500 text-white rounded hover:bg-blue-600 z-10"
          onClick={() => {
            console.log("Opening dialog");
            setShowDialog(true);
          }}
        >
          创建聊天室
        </button>

        <div className="flex-1 overflow-y-auto" ref={roomsRef}>
          {rooms.map(({ id, name, messages }) => (
            <RoomEntry
              key={id}
              id={id}
              name={name}
              selected={id === currentRoom?.id}
              lastMessage={messages[0]?.content || "No messages yet"}
              onClick={onClickRoom}
            />
          ))}
        </div>
      </div>

      {showDialog && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
          <div className="bg-white p-4 rounded-lg min-w-[300px] flex flex-col">
            <h3 className="text-lg mb-4 text-center">创建新聊天室</h3>
            <div className="flex-1 flex items-center justify-center mb-4">
              <input
                type="text"
                value={roomName}
                onChange={(e) => setRoomName(e.target.value)}
                className="border p-2 w-full rounded"
                placeholder="输入聊天室名称"
                autoFocus
              />
            </div>
            <div className="flex justify-end gap-2">
              <button
                className="px-4 py-2 bg-gray-200 rounded hover:bg-gray-300"
                onClick={() => {
                  console.log("Closing dialog");
                  setShowDialog(false);
                }}
              >
                取消
              </button>
              <button
                className="px-4 py-2 bg-blue-500 text-white rounded hover:bg-blue-600"
                onClick={handleCreateRoom}
              >
                确定
              </button>
            </div>
          </div>
        </div>
      )}

      <div className="flex-[3] flex flex-col">
        <div
          className="flex-1 flex flex-col-reverse p-5 overflow-y-auto"
          style={{ 
            backgroundColor: "var(--boost-light-grey)",
            height: "calc(100vh - 200px)",
            overflowY: "auto",
            scrollBehavior: "smooth"
          }}
          ref={messagesContainerRef}
          onWheel={handleScroll}
        >
          <div className="flex flex-col-reverse">
            <div ref={messagesEndRef} />
            
            {/* 消息列表 */}
            {currentRoomMessages.length === 0 ? (
              <p>No more messages to show</p>
            ) : (
              currentRoomMessages.map((msg) => (
                <Message
                  key={msg.id}
                  userId={msg.user.id}
                  username={msg.user.username}
                  content={msg.content}
                  timestamp={msg.timestamp}
                  currentUserId={currentUserId}
                />
              ))
            )}
            
            {/* 历史记录提示 */}
            {currentRoom && (
              <div 
                className={`text-center py-2 text-gray-500 text-sm transition-all duration-300 ease-in-out ${
                  isLoading ? 'opacity-50' : 'opacity-100'
                }`}
              >
                {isLoading ? (
                  <div className="flex items-center justify-center gap-2">
                    <svg className="animate-spin h-4 w-4 text-gray-500" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
                      <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                      <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
                    </svg>
                    <span>加载历史消息中...</span>
                  </div>
                ) : currentRoom.hasMoreMessages ? (
                  <div className="animate-bounce">
                    <p>👆 向上滑动加载更多消息</p>
                  </div>
                ) : (
                  <p className="transition-opacity duration-300">已经到达最早的消息</p>
                )}
              </div>
            )}
          </div>
        </div>
        <MessageInputBar onMessage={onMessage} />
      </div>
    </div>
  );
};


// Websocket close code to signal that authentication is required
const CODE_POLICY_VIOLATION = 1008;

// The actual component
export default function ChatPage() {
  // State management
  const [state, dispatch] = useReducer(reducer, {
    currentUser: null,
    loading: true,
    rooms: {},
    currentRoomId: null,
  });

  // Send a websocket message to the server when the user hits enter
  const onMessageTyped = useCallback(
    (msg: string) => {
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
    const evt = serializeCreateRoomEvent(roomName);
    websocketRef.current.send(evt);
  }, []);

  const router = useRouter();

  // Handle server websocket messages
  type WebSocketMessageType = "hello" | "serverMessages" | "serverCreateRoom" | "serverRoomHistory";

  // 添加历史记录请求处理函数
  const onRequestHistory = useCallback((roomId: string, firstMessageId: string) => {
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
        dispatch({
          type: "add_new_room",
          payload: {
            roomId: payload.roomId,
            roomName: payload.roomName,
          },
        });
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
  }, []);

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

  // Create a websocket
  const websocketRef = useRef<WebSocket>(null);
  useEffect(() => {
    websocketRef.current = new WebSocket(getWebsocketURL());
    websocketRef.current.addEventListener("message", onWebsocketMessage);
    websocketRef.current.addEventListener("close", onClose);

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
  const rooms = Object.values(state.rooms);
  sortRoomsByLastMessageInplace(rooms);

  return (
    <>
      <Head />
      <div className="flex flex-col h-full">
        <Header />
        <ChatScreen
          rooms={rooms}
          currentRoom={currentRoom}
          currentUserId={state.currentUser.id}
          onClickRoom={onClickRoom}
          onMessage={onMessageTyped}
          onCreateRoom={onCreateRoom}
          onRequestHistory={onRequestHistory}
        />
      </div>
    </>
  );
}
