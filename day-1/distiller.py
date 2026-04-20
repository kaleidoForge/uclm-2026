import tensorflow as tf

class Distiller(tf.keras.Model):
    def __init__(self, student, teacher):
        super().__init__()
        self.student = student
        self.teacher = teacher

    def compile(self, optimizer, metrics, student_loss_fn,
                distillation_loss_fn, alpha=0.1, temperature=4):
        super().compile(optimizer=optimizer, metrics=metrics)
        self.student_loss_fn      = student_loss_fn
        self.distillation_loss_fn = distillation_loss_fn
        self.alpha       = alpha
        self.temperature = temperature

    def train_step(self, data):
        x, y = data
        t_logits = self.teacher(x, training=False)
        with tf.GradientTape() as tape:
            s_logits = self.student(x, training=True)
            s_loss   = self.student_loss_fn(y, s_logits)
            d_loss   = self.distillation_loss_fn(
                tf.nn.softmax(t_logits / self.temperature, axis=1),
                tf.nn.softmax(s_logits / self.temperature, axis=1),
            )
            loss = self.alpha * s_loss + (1 - self.alpha) * d_loss
        grads = tape.gradient(loss, self.student.trainable_variables)
        self.optimizer.apply_gradients(zip(grads, self.student.trainable_variables))
        self.compiled_metrics.update_state(y, s_logits)
        results = {m.name: m.result() for m in self.metrics}
        results.update({'student_loss': s_loss, 'distillation_loss': d_loss})
        return results

    def test_step(self, data):
        x, y = data
        y_pred = self.student(x, training=False)
        s_loss = self.student_loss_fn(y, y_pred)
        self.compiled_metrics.update_state(y, y_pred)
        results = {m.name: m.result() for m in self.metrics}
        results.update({'student_loss': s_loss})
        return results