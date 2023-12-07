from flask import Flask, render_template
from flask_socketio import SocketIO, emit
from flask import jsonify,request
app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret'
socketio = SocketIO(app)
# Обработчик маршрута "/"
@app.route('/')
def index():
    return render_template('index_2.html')
if __name__ == '__main__':
    socketio.run(app, host='127.0.0.1', port=8095, debug=True)