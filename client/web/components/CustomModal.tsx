import React, { useState } from 'react';
import Modal from 'react-modal';

Modal.setAppElement('#__next'); // 设置应用的根元素

const CustomModal = ({ isOpen, onRequestClose, onMessage }) => {
  const [inputValue, setInputValue] = useState('');

  const handleConfirm = () => {
    if (inputValue) {
      onMessage(inputValue);
      setInputValue('');
      onRequestClose();
    }
  };

  return (
    <Modal
      isOpen={isOpen}
      onRequestClose={onRequestClose}
      contentLabel="Message Input Modal"
    >
      <h2>Input Data</h2>
      <input
        type="text"
        value={inputValue}
        onChange={(e) => setInputValue(e.target.value)}
      />
      <button onClick={handleConfirm}>Confirm</button>
      <button onClick={onRequestClose}>Cancel</button>
    </Modal>
  );
};

export default CustomModal;