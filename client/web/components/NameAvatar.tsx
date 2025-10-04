// An avatar that consists of the first letter of a name

const colors = [
    { bg: 'bg-red-200', text: 'text-red-800' },
    { bg: 'bg-green-200', text: 'text-green-800' },
    { bg: 'bg-blue-200', text: 'text-blue-800' },
    { bg: 'bg-yellow-200', text: 'text-yellow-800' },
    { bg: 'bg-indigo-200', text: 'text-indigo-800' },
    { bg: 'bg-pink-200', text: 'text-pink-800' },
    { bg: 'bg-purple-200', text: 'text-purple-800' },
    { bg: 'bg-orange-200', text: 'text-orange-800' },
    { bg: 'bg-teal-200', text: 'text-teal-800' },
];

function getAvatarColor(name: string) {
    if (!name) return colors[0];
    let hash = 0;
    for (let i = 0; i < name.length; i++) {
        hash = name.charCodeAt(i) + ((hash << 5) - hash);
        hash = hash & hash; // Convert to 32bit integer
    }
    const index = Math.abs(hash % colors.length);
    return colors[index];
}

export default function NameAvatar({ name }: { name: string }) {
  const color = getAvatarColor(name);
  return (
    <div className={`w-10 h-10 rounded-full ${color.bg} flex items-center justify-center ${color.text} text-xl font-bold shadow-sm`}>
      {name.charAt(0).toUpperCase()}
    </div>
  );
}