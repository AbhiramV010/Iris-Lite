import tensorflow as tf
from tensorflow.keras import layers, models
import numpy as np

def defineModel():
    model = models.Sequential([
        layers.Input(shape=(128, 98, 1)),
        
        layers.Conv2D(32, (3, 3), activation='relu'),
        layers.MaxPooling2D((2, 2)),
        layers.Conv2D(64, (3, 3), activation='relu'),
        layers.MaxPooling2D((2, 2)),
        
        layers.Flatten(),
        layers.Dense(64, activation='relu'),
        layers.Dropout(0.3), # Prevents overfitting 
        layers.Dense(8, activation='softmax') # 8 UNIQUE sounds, sound_0 to sound_7
    ])
    return model 